defmodule AnankeWeb.Router do
  use AnankeWeb, :router

  pipeline :browser do
    plug(:accepts, ["html"])
    plug(:fetch_session)
    plug(:fetch_flash)
    plug(:protect_from_forgery)
    plug(:put_secure_browser_headers)
  end

  pipeline :api do
    plug(:accepts, ["json"])
  end

  scope "/", AnankeWeb do
    # Use the default browser stack
    pipe_through(:browser)

    get("/", PageController, :index)
  end

  forward("/share", Ananke.SharePlug)
  # Other scopes may use custom stacks.
  # scope "/api", AnankeWeb do
  #   pipe_through :api
  # end
end
