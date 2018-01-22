open Webapi.Dom;

let offset = 8_000_000;

let unwrap =
  fun
  | Some(v) => v
  | None => raise(Invalid_argument("Passed `None` to unwrap"));

let make_req_body = (name, size) =>
  Json.Encode.(object_([("name", string(name)), ("size", int(size))]))
  |> Json.stringify
  |> Fetch.BodyInit.make;

let get_location = resp =>
  Fetch.Response.headers(resp)
  |> Fetch.Headers.get("location")
  |> unwrap
  |> Js.Promise.resolve;

let rec upload_part = (file, size, start, end', location) => {
  let end' = size > end' ? end' : size;
  let body = file |> File.slice(start, end') |> Fetch.BodyInit.makeWithBlob;
  Js.Promise.(
    Fetch.fetchWithInit(
      location,
      Fetch.RequestInit.make(~method_=Put, ~body, ())
    )
    |> then_(_resp => send_file(file, size, end', end' + offset, location))
  );
}
and send_file = (file, size, start, end', location) =>
  size == start ?
    Js.log("Uploaded!") |> Js.Promise.resolve :
    upload_part(file, size, start, end', location);

let init = (file, size, body, headers) =>
  Js.Promise.(
    Fetch.fetchWithInit(
      "/upload",
      Fetch.RequestInit.make(~method_=Post, ~headers, ~body, ())
    )
    |> then_(get_location)
    |> then_(send_file(file, size, 0, offset))
  );

let start_upload = (file, name, size) => {
  let body = make_req_body(name, size);
  let headers = Fetch.HeadersInit.make({"content-type": "application/json"});
  let _ = init(file, size, body, headers);
  ();
};

let upload = file => {
  let name = File.get_name(file);
  let size = File.get_size(file);
  start_upload(file, name, size);
};

let upload_file = () =>
  document
  |> Document.getElementById("upload")
  |> unwrap
  |> File.get_files
  |> Array.iter(upload);

document
|> Document.getElementById("upload")
|> unwrap
|> Element.addEventListener("change", _evt => upload_file());
