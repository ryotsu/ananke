defmodule AnankeWeb.PageController do
  use AnankeWeb, :controller

  def index(conn, _params) do
    render conn, "index.html"
  end
end
