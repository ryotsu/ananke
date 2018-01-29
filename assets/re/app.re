open Webapi.Dom;

let offset = 8_000_000;

let unwrap =
  fun
  | Some(v) => v
  | None => raise(Invalid_argument("Passed `None` to unwrap"));

let map = f =>
  fun
  | Some(v) => Some(f(v))
  | None => None;

let toggle_button = id => {
  let _ =
    document
    |> Document.getElementById(id)
    |> map(Element.classList)
    |> map(DomTokenList.toggle("pure-button-disabled"));
  ();
};

let make_req_body = (name, size) =>
  Json.Encode.(object_([("name", string(name)), ("size", int(size))]))
  |> Json.stringify
  |> Fetch.BodyInit.make;

let show_link = location => {
  let elem = Document.getElementById("link", document) |> unwrap;
  let _ = Element.setInnerHTML(elem, location);
  Js.Promise.resolve(location);
};

let get_location = resp =>
  Fetch.Response.headers(resp)
  |> Fetch.Headers.get("location")
  |> map(Js.Promise.resolve)
  |> unwrap;

let update_progress = uploaded =>
  document
  |> Document.getElementById("upload-progress")
  |> map(Element.setAttribute("value", uploaded |> string_of_int));

let rec upload_blob = (file, size, start, end', location) => {
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
and send_file = (file, size, start, end', location) => {
  let _ = update_progress(start);
  size == start ?
    {
      toggle_button("file-select");
      Js.log("Uploaded!") |> Js.Promise.resolve;
    } :
    upload_blob(file, size, start, end', location);
};

let init = (file, size, body, headers) =>
  Js.Promise.(
    Fetch.fetchWithInit(
      "/upload",
      Fetch.RequestInit.make(~method_=Post, ~headers, ~body, ())
    )
    |> then_(get_location)
    |> then_(show_link)
    |> then_(send_file(file, size, 0, offset))
  );

let start_upload = (file, name, size) => {
  let body = make_req_body(name, size);
  let headers = Fetch.HeadersInit.make({"content-type": "application/json"});
  let _ = init(file, size, body, headers);
  ();
};

let init_progress_bar = size => {
  let elem = document |> Document.getElementById("upload-progress") |> unwrap;
  let _ = Element.removeAttribute("hidden", elem);
  let _ = Element.setAttribute("value", "0", elem);
  let _ = Element.setAttribute("max", size |> string_of_int, elem);
  ();
};

let upload = file => {
  let name = File.get_name(file);
  let size = File.get_size(file);
  let _ = init_progress_bar(size);
  start_upload(file, name, size);
};

let upload_file = () =>
  document
  |> Document.getElementById("file-upload")
  |> map(File.get_files)
  |> map(Array.iter(upload))
  |> unwrap;

document
|> Document.getElementById("file-upload")
|> map(Element.addEventListener("change", _evt => toggle_button("upload")));

document
|> Document.getElementById("upload")
|> map(
     Element.addEventListener("click", _evt => {
       toggle_button("file-select");
       toggle_button("upload");
       upload_file();
     })
   );
