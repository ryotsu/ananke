defmodule Ananke.SharePlug do
  import Plug.Conn

  alias Ananke.Manager
  alias Ananke.Upload

  def init(opts) do
    opts
    |> Keyword.put_new(:share, "/share")
  end

  def call(%Plug.Conn{method: method} = conn, opts) when method == "POST" do
    share_url = Keyword.fetch!(opts, :share)

    case initiate_upload(conn) do
      {:ok, url} ->
        conn
        |> put_resp_header("location", share_url <> "/" <> url)
        |> send_resp(:ok, "")

      :error ->
        send_resp(conn, :bad_request, "")
    end
  end

  def call(%Plug.Conn{method: method, path_info: [path]} = conn, opts) when method == "PUT" do
    case handle_upload(path, conn, opts) do
      {:ok, conn, status, size} ->
        conn
        |> put_resp_header("range", "bytes=0-#{size}")
        |> send_resp(status, "")

      :error ->
        send_resp(conn, :bad_request, "")
    end
  end

  def call(%Plug.Conn{method: method, path_info: [path]} = conn, opts) when method == "GET" do
    case handle_download(path, conn, opts) do
      :error ->
        send_resp(conn, :not_found, "")

      conn ->
        conn
    end
  end

  def call(conn, _opts) do
    send_resp(conn, :not_found, "")
  end

  @spec initiate_upload(Plug.Conn.t()) :: {:ok, String.t()} | :error
  defp initiate_upload(%Plug.Conn{body_params: params}) do
    with {:ok, name} <- Map.fetch(params, "name"),
         {:ok, size} <- Map.fetch(params, "size") do
      size = if is_integer(size), do: size, else: String.to_integer(size)

      url = Manager.new(name, size)
      {:ok, url}
    end
  end

  @spec handle_upload(String.t(), Plug.Conn.t(), list) ::
          {:ok, Plug.Conn.t(), Plug.Conn.status(), integer} | :error
  defp handle_upload(path, conn, opts) do
    with {:ok, file} <- Manager.get_file(path),
         {conn, status, file} <- Upload.write(file, conn, opts),
         :ok <- Manager.save_file(path, file) do
      {:ok, conn, status, file.uploaded}
    end
  end

  @spec handle_download(String.t(), Plug.Conn.t(), list) :: Plug.Conn.t() | :error
  defp handle_download(path, conn, _opts) do
    with {:ok, file} <- Manager.download_file(path) do
      download(conn, file)
    end
  end

  @spec download(Plug.Conn.t(), Upload.t()) :: Plug.Conn.t()
  defp download(conn, %Upload{size: size, uploaded: uploaded} = upload) when size == uploaded do
    conn
    |> set_headers(upload.name)
    |> send_file(:ok, upload.path)
  end

  defp download(conn, %Upload{size: size, uploaded: uploaded} = upload) do
    conn
    |> set_headers(upload.name)
    |> put_resp_header("content-length", size |> Integer.to_string())
    |> send_chunked(:ok)
    |> start_chunk_download(upload.path, size, uploaded)
  end

  @spec set_headers(Plug.Conn.t(), String.t()) :: Plug.Conn.t()
  defp set_headers(conn, name) do
    conn
    |> put_resp_content_type("application/octet-stream")
    |> put_resp_header("content-transfer-encoding", "binary")
    |> put_resp_header("content-disposition", ~s[attachment; filename="#{name}"])
  end

  @spec start_chunk_download(Plug.Conn.t(), String.t(), integer, integer) :: Plug.Conn.t()
  defp start_chunk_download(conn, path, size, uploaded) do
    {:ok, file} = File.open(path, [:read, :binary, :raw])
    data = IO.binread(file, uploaded)
    {:ok, conn} = chunk(conn, data)
    chunk_download(conn, file, size, uploaded)
  end

  @spec chunk_download(Plug.Conn.t(), :file.io_device(), integer, integer) :: Plug.Conn.t()
  defp chunk_download(conn, file, size, uploaded) when size != uploaded do
    receive do
      {:next, up} ->
        data = IO.binread(file, up - uploaded)

        case chunk(conn, data) do
          {:ok, conn} ->
            chunk_download(conn, file, size, up)

          {:error, _reason} ->
            File.close(file)
            conn
        end
    end
  end

  defp chunk_download(conn, file, _, _) do
    File.close(file)
    conn
  end
end
