defmodule Ananke.SharePlug do
  import Plug.Conn

  alias Ananke.Manager
  alias Ananke.Upload

  def init(opts) do
    opts
    |> Keyword.put_new(:upload, "/upload")
    |> Keyword.put_new(:download, "/download")
  end

  def call(%Plug.Conn{method: method} = conn, opts) when method == "POST" do
    upload_url = Keyword.fetch!(opts, :upload)

    case initiate_upload(conn) do
      {:ok, url} ->
        conn
        |> put_resp_header("location", upload_url <> "/" <> url)
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

  def call(conn, _opts) do
    send_resp(conn, :not_found, "")
  end

  defp initiate_upload(%Plug.Conn{body_params: params}) do
    with {:ok, name} <- Map.fetch(params, "name"),
         {:ok, size} <- Map.fetch(params, "size") do
      size = if is_integer(size), do: size, else: String.to_integer(size)

      url = Manager.new(name, size)
      {:ok, url}
    end
  end

  defp handle_upload(path, conn, opts) do
    with {:ok, file} <- Manager.get_file(path),
         {conn, status, file} <- Upload.write(file, conn, opts),
         :ok <- Manager.save_file(path, file) do
      {:ok, conn, status, file.uploaded}
    end
  end
end
