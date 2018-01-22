defmodule Ananke.Upload do
  @type t :: %__MODULE__{
          name: String.t(),
          url: String.t(),
          path: Path.t(),
          size: integer,
          uploaded: integer
        }

  defstruct [:name, :url, :path, :size, uploaded: 0]

  @spec write(t, Plug.Conn.t(), list) :: {Plug.Conn.t(), :accepted | :created, t}
  def write(%__MODULE__{path: path, uploaded: uploaded, size: size} = upload, conn, opts) do
    {:ok, file} = File.open(path, [:read, :write, :binary, :delayed_write, :raw])
    {:ok, _pos} = :file.position(file, {:eof, 0})

    {status, remaining, conn} =
      write_file(Plug.Conn.read_body(conn, opts), file, size - uploaded, opts)

    :ok = File.close(file)
    upload = %__MODULE__{upload | uploaded: size - remaining}

    {conn, status, upload}
  end

  @spec write_file({:ok | :more, binary, Plug.Conn.t()}, File.io_device(), integer, list) ::
          {:created | :accepted, integer, Plug.Conn.t()}
  defp write_file({_, body, conn}, file, remaining, _opts) when byte_size(body) >= remaining do
    <<data::binary-size(remaining), _rest::binary>> = body
    IO.binwrite(file, data)
    {:created, 0, conn}
  end

  defp write_file({:ok, body, conn}, file, remaining, _opts) do
    IO.binwrite(file, body)
    {:accepted, remaining - byte_size(body), conn}
  end

  defp write_file({:more, body, conn}, file, remaining, opts) do
    IO.binwrite(file, body)
    write_file(Plug.Conn.read_body(conn, opts), file, remaining - byte_size(body), opts)
  end
end
