open Utils;

open Antd;

type action =
  | Select(file)
  | FetchUrl
  | Start(string, string)
  | Continue(int)
  | Error(string);

type status =
  | NotSelected
  | Selected
  | Uploading
  | Finished
  | Failed;

type state = {
  status,
  file: option(file),
  url: option(string),
  key: option(string),
  uploaded: int
};

let offset = 500_000;

let get_header_value = (header, resp) =>
  Fetch.Response.headers(resp) |> Fetch.Headers.get(header) |> unwrap;

let make_init_req_body = (name, size) =>
  Json.Encode.(object_([("name", string(name)), ("size", int(size))]))
  |> Json.stringify
  |> Fetch.BodyInit.make;

let check_resp = (start_upload, send_error, resp) =>
  (
    switch (Fetch.Response.status(resp)) {
    | 200 =>
      let url = get_header_value("location", resp);
      let key = get_header_value("x-key", resp);
      start_upload(url, key);
    | 400 =>
      Fetch.Response.text(resp)
      |> Js.Promise.then_(error => {
           send_error(error);
           Js.Promise.resolve();
         })
      |> ignore
    | _ => send_error("Some error occurred")
    }
  )
  |> Js.Promise.resolve;

let fetch_url = ({name, size}, start_upload, send_error) => {
  let body = make_init_req_body(name, size);
  let headers = Fetch.HeadersInit.make({"content-type": "application/json"});
  Js.Promise.(
    Fetch.fetchWithInit(
      "/share",
      Fetch.RequestInit.make(~method_=Post, ~headers, ~body, ())
    )
    |> then_(check_resp(start_upload, send_error))
    |> ignore
  );
};

let upload_blob = ({file, size}, start, end', url, key, continue) => {
  let end' = size > end' ? end' : size;
  let body = file |> slice(start, end') |> Fetch.BodyInit.makeWithBlob;
  let headers = Fetch.HeadersInit.make({"x-key": key});
  Js.Promise.(
    Fetch.fetchWithInit(
      url,
      Fetch.RequestInit.make(~method_=Put, ~headers, ~body, ())
    )
    |> then_(_resp => {
         continue(end');
         resolve();
       })
    |> ignore
  );
};

let beforeUpload = (send, file) => {
  let file = {file, name: file |> get_file_name, size: file |> get_file_size};
  send(Select(file));
  Js.false_;
};

let component = ReasonReact.reducerComponent("Page");

let make = _children => {
  ...component,
  initialState: () => {
    status: NotSelected,
    file: None,
    url: None,
    key: None,
    uploaded: 0
  },
  reducer: (action, {file, url, key} as state) =>
    switch action {
    | Select(file) =>
      ReasonReact.Update({...state, status: Selected, file: Some(file)})
    | FetchUrl =>
      ReasonReact.UpdateWithSideEffects(
        {...state, status: Uploading, url: None, key: None, uploaded: 0},
        (
          self =>
            fetch_url(
              unwrap(file),
              (url, key) => self.send(Start(url, key)),
              error => self.send(Error(error))
            )
        )
      )
    | Start(url, key) =>
      ReasonReact.UpdateWithSideEffects(
        {...state, url: Some(url), key: Some(key)},
        (
          self =>
            upload_blob(unwrap(file), 0, offset, url, key, end' =>
              self.send(Continue(end'))
            )
        )
      )
    | Continue(start) =>
      start == unwrap(file).size ?
        ReasonReact.UpdateWithSideEffects(
          {...state, status: Finished, uploaded: start},
          (_self => Message.success("Uploaded successfully", 3))
        ) :
        ReasonReact.UpdateWithSideEffects(
          {...state, uploaded: start},
          (
            self =>
              upload_blob(
                unwrap(file),
                start,
                start + offset,
                unwrap(url),
                unwrap(key),
                end' =>
                self.send(Continue(end'))
              )
          )
        )
    | Error(error) =>
      ReasonReact.UpdateWithSideEffects(
        {...state, status: Failed},
        (_self => Message.error(error, 3))
      )
    },
  render: ({state: {file, status, uploaded, url}, send}) =>
    <div className="centered">
      <div className="box">
        (
          (status == Uploading || status == Finished) && url != None ?
            <Button href=(unwrap(url)) icon="download">
              (str("Download"))
            </Button> :
            ReasonReact.nullElement
        )
      </div>
      <div className="box">
        <Upload beforeUpload=(beforeUpload(send))>
          <Button
            type_="primary"
            size="large"
            icon="file"
            disabled=(status == Uploading)>
            (str("Select File"))
          </Button>
        </Upload>
        <Button
          type_="primary"
          size="large"
          icon="upload"
          disabled=(status != Selected && status != Failed)
          loading=(status == Uploading |> Js.Boolean.to_js_boolean)
          onClick=(_evt => send(FetchUrl))>
          (str("Upload File"))
        </Button>
      </div>
      (
        status != NotSelected && status != Selected ?
          ReasonReact.cloneElement(
            <Progress
              percent=(
                (uploaded |> float_of_int)
                /. (unwrap(file).size |> float_of_int)
                *. 100.0
                |> int_of_float
              )
              type_="line"
            />,
            ~props=
              switch status {
              | Uploading => {"status": "active"}
              | Failed => {"status": "exception"}
              | _ => {"status": "success"}
              },
            [||]
          ) :
          ReasonReact.nullElement
      )
    </div>
};
