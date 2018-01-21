defmodule Ananke.Manager do
  use GenServer

  alias Ananke.Upload

  @table __MODULE__

  def start_link do
    GenServer.start_link(__MODULE__, :ok, name: __MODULE__)
  end

  @spec new(String.t(), integer) :: String.t()
  def new(name, size) do
    GenServer.call(__MODULE__, {:new, name, size})
  end

  @spec get_file(String.t()) :: {:ok, Upload.t()} | :error
  def get_file(url) do
    GenServer.call(__MODULE__, {:get, url})
  end

  @spec save_file(String.t(), Upload.t()) :: :ok
  def save_file(url, file) do
    GenServer.call(__MODULE__, {:save, url, file})
  end

  def init(:ok) do
    Process.flag(:trap_exit, true)
    :ets.new(@table, [:named_table, :public, :set])
    tmp = Path.join(File.cwd!(), "tmp")
    :ok = File.mkdir_p(tmp)
    {:ok, {tmp, []}}
  end

  def handle_call({:new, name, size}, _from, {tmp, _opened} = state) do
    file = create(name, size, tmp)
    true = :ets.insert_new(@table, {file.url, file})
    {:reply, file.url, state}
  end

  def handle_call({:get, url}, _from, {tmp, opened}) do
    case {:ets.lookup(@table, url), url in opened} do
      {[{^url, file}], false} ->
        {:reply, {:ok, file}, {tmp, [url | opened]}}

      _ ->
        {:reply, :error, {tmp, opened}}
    end
  end

  def handle_call({:save, url, file}, _from, {tmp, opened}) do
    :ets.insert(@table, {url, file})
    opened = List.delete(opened, url)
    {:reply, :ok, {tmp, opened}}
  end

  def terminate(_reason, {tmp, _opened}) do
    :ets.foldl(fn {_url, file}, _ -> File.rm(file.path) end, :ok, @table)
    File.rmdir(tmp)
    :ok
  end

  @spec create(String.t(), integer, Path.t()) :: Upload.t()
  defp create(name, size, tmp) do
    url = :crypto.strong_rand_bytes(12) |> Base.url_encode64()
    filename = :crypto.strong_rand_bytes(12) |> Base.url_encode64()
    path = Path.join(tmp, filename)
    :ok = File.touch(path)

    %Upload{
      name: name,
      url: url,
      path: path,
      size: size
    }
  end
end
