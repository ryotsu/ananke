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

  @spec download_file(String.t()) :: {:ok, Upload.t()} | :error
  def download_file(url) do
    GenServer.call(__MODULE__, {:download, url})
  end

  def init(:ok) do
    Process.flag(:trap_exit, true)
    :ets.new(@table, [:named_table, :public, :set])
    tmp = Path.join(File.cwd!(), "tmp")
    :ok = File.mkdir_p(tmp)
    {:ok, %{tmp: tmp, opened: [], downloaders: %{}, pids: %{}}}
  end

  def handle_call({:new, name, size}, _from, %{tmp: tmp} = state) do
    file = create(name, size, tmp)
    true = :ets.insert_new(@table, {file.url, file})
    {:reply, file.url, state}
  end

  def handle_call({:get, url}, _from, %{opened: opened} = state) do
    case {:ets.lookup(@table, url), url in opened} do
      {[{^url, file}], false} ->
        {:reply, {:ok, file}, %{state | opened: [url | opened]}}

      _ ->
        {:reply, :error, state}
    end
  end

  def handle_call({:save, url, file}, _from, %{downloaders: dlers, opened: opened} = state) do
    :ets.insert(@table, {url, file})
    for pid <- Map.get(dlers, url, []), do: send(pid, {:next, file.uploaded})
    {:reply, :ok, %{state | opened: List.delete(opened, url)}}
  end

  def handle_call({:download, url}, {pid, _tag}, %{downloaders: dlrs, pids: pids} = state) do
    case :ets.lookup(@table, url) do
      [{^url, file}] ->
        Process.monitor(pid)
        dlrs = Map.update(dlrs, url, [pid], fn pids -> [pid | pids] end)
        pids = Map.put(pids, pid, url)
        {:reply, {:ok, file}, %{state | downloaders: dlrs, pids: pids}}

      _ ->
        {:reply, :error, state}
    end
  end

  def handle_info({:DOWN, _ref, :process, pid, _rsn}, %{downloaders: dlrs, pids: pids} = state) do
    {url, pids} = Map.pop(pids, pid)
    dlrs = Map.update(dlrs, url, [], &List.delete(&1, pid))
    {:noreply, %{state | downloaders: dlrs, pids: pids}}
  end

  def terminate(_reason, %{tmp: tmp}) do
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
