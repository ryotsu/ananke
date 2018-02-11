defmodule Ananke.Manager do
  use GenServer

  alias Ananke.Upload

  @table __MODULE__

  @delay 30 * 60 * 1000

  def start_link do
    GenServer.start_link(__MODULE__, :ok, name: __MODULE__)
  end

  @spec new(String.t(), integer) :: {String.t(), String.t()}
  def new(name, size) do
    GenServer.call(__MODULE__, {:new, name, size})
  end

  @spec get_file(String.t(), String.t()) :: {:ok, Upload.t()} | :error
  def get_file(url, key) do
    GenServer.call(__MODULE__, {:get, url, key})
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
    Process.send_after(self(), :clear_files, @delay)
    {:ok, %{tmp: tmp, opened: [], downloaders: %{}}}
  end

  def handle_call({:new, name, size}, _from, %{tmp: tmp} = state) do
    file = create(name, size, tmp)
    true = :ets.insert_new(@table, {file.url, file})
    {:reply, {file.url, file.key}, state}
  end

  def handle_call({:get, url, key}, _from, %{opened: opened} = state) do
    case {:ets.lookup(@table, url), url in opened} do
      {[{^url, %Upload{key: ^key} = file}], false} ->
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

  def handle_call({:download, url}, {pid, _tag}, %{downloaders: downloaders} = state) do
    case :ets.lookup(@table, url) do
      [{^url, file}] ->
        ref = Process.monitor(pid)

        downloaders =
          downloaders
          |> Map.update(url, [pid], fn pids -> [pid | pids] end)
          |> Map.put(pid, {url, ref})

        {:reply, {:ok, file}, %{state | downloaders: downloaders}}

      _ ->
        {:reply, :error, state}
    end
  end

  def handle_info(:clear_files, %{opened: opened, downloaders: downloaders} = state) do
    downloaders =
      :ets.foldl(
        fn {url, file}, downloaders ->
          remove_files(url, file, opened, downloaders)
        end,
        downloaders,
        @table
      )

    Process.send_after(self(), :clear_files, @delay)
    {:noreply, %{state | downloaders: downloaders}}
  end

  def handle_info({:DOWN, _ref, :process, pid, _rsn}, %{downloaders: downloaders} = state) do
    {{url, ref}, downloaders} = Map.pop(downloaders, pid)
    Process.demonitor(ref)
    downloaders = Map.update(downloaders, url, [], &List.delete(&1, pid))
    {:noreply, %{state | downloaders: downloaders}}
  end

  def terminate(_reason, %{tmp: tmp}) do
    :ets.foldl(fn {_url, file}, _ -> File.rm(file.path) end, :ok, @table)
    File.rmdir(tmp)
    :ok
  end

  @spec create(String.t(), integer, Path.t()) :: Upload.t()
  defp create(name, size, tmp) do
    url = :crypto.strong_rand_bytes(12) |> Base.url_encode64()
    key = :crypto.strong_rand_bytes(12) |> Base.url_encode64()
    filename = :crypto.strong_rand_bytes(12) |> Base.url_encode64()
    path = Path.join(tmp, filename)
    :ok = File.touch(path)

    %Upload{
      name: name,
      url: url,
      key: key,
      path: path,
      created: Time.utc_now(),
      size: size
    }
  end

  @spec remove_files(String.t(), Upload.t(), [String.t()], map) :: map
  defp remove_files(url, file, opened, downloaders) do
    case {Time.diff(Time.utc_now(), file.created, :millisecond) > @delay, url in opened} do
      {true, false} ->
        {pids, downloaders} = Map.pop(downloaders, url, [])
        File.rm(file.path)
        :ets.delete(@table, url)

        Enum.reduce(pids, downloaders, fn pid, dl ->
          {{^url, ref}, dl} = Map.pop(dl, pid)
          Process.demonitor(ref)
          dl
        end)

      _ ->
        downloaders
    end
  end
end
