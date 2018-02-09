open Utils;

type action =
  | Select(file)
  | FetchUrl
  | Upload(string, string)
  | ContinueUpload(string, int)
  | ShowError(string);

type upload = {
  file,
  url: string,
  key: string,
  uploaded: int
};

type state =
  | NotSelected
  | Selected(file)
  | Error(string, file)
  | InitiatedUpload(file)
  | Uploading(upload)
  | Uploaded(upload);

let offset = 8_000_000;

let get_file = event : file => {
  let files = (event |> ReactEventRe.Form.target |> ReactDOMRe.domElementToObj)##files;
  {
    file: files[0],
    name: files[0] |> get_file_name,
    size: files[0] |> get_file_size
  };
};

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

let fetch_url = (name, size, start_upload, send_error) => {
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

let upload_blob = (file, size, start, end', url, key, continue) => {
  let end' = size > end' ? end' : size;
  let body = file |> slice(start, end') |> Fetch.BodyInit.makeWithBlob;
  let headers = Fetch.HeadersInit.make({"x-key": key});
  Js.Promise.(
    Fetch.fetchWithInit(
      url,
      Fetch.RequestInit.make(~method_=Put, ~headers, ~body, ())
    )
    |> then_(_resp => {
         continue(url, end');
         resolve();
       })
    |> ignore
  );
};

let component = ReasonReact.reducerComponent("Page");

let make = _children => {
  ...component,
  initialState: () => NotSelected,
  reducer: (action, state) =>
    switch (action, state) {
    | (Select(file), _) => ReasonReact.Update(Selected(file))
    | (FetchUrl, Error(_, {name, size} as file))
    | (FetchUrl, Selected({name, size} as file)) =>
      ReasonReact.UpdateWithSideEffects(
        InitiatedUpload(file),
        (
          self =>
            fetch_url(
              name,
              size,
              (url, key) => self.send(Upload(url, key)),
              error => self.send(ShowError(error))
            )
        )
      )
    | (Upload(url, key), InitiatedUpload(file)) =>
      ReasonReact.UpdateWithSideEffects(
        Uploading({file, url, key, uploaded: 0}),
        (
          self =>
            upload_blob(file.file, file.size, 0, offset, url, key, (url, end') =>
              self.send(ContinueUpload(url, end'))
            )
        )
      )
    | (ContinueUpload(url, up), Uploading({file: {file, size}, key} as upload)) =>
      up == size ?
        ReasonReact.Update(Uploaded({...upload, uploaded: up})) :
        ReasonReact.UpdateWithSideEffects(
          Uploading({...upload, uploaded: up}),
          (
            self =>
              upload_blob(file, size, up, up + offset, url, key, (url, end') =>
                self.send(ContinueUpload(url, end'))
              )
          )
        )
    | (ShowError(error), InitiatedUpload(file)) =>
      ReasonReact.Update(Error(error, file))
    | (_, _) => ReasonReact.NoUpdate
    },
  render: ({state, send}) =>
    <div className="centered">
      (
        switch state {
        | Uploading({url})
        | Uploaded({url}) =>
          <a href=url target="_blank"> (str("Download File")) </a>
        | Error(error, _) => <span> (str(error)) </span>
        | _ => ReasonReact.nullElement
        }
      )
      <div className="box">
        <div
          className=(
            "file-upload pure-button pure-button-primary"
            ++ (
              switch state {
              | InitiatedUpload(_)
              | Uploading(_) => " pure-button-disabled"
              | _ => ""
              }
            )
          )>
          <i className="fa fa-file" />
          <span> (str("Select File")) </span>
          <input _type="file" onChange=(evt => send(Select(get_file(evt)))) />
        </div>
        <div
          className=(
            "pure-button button-secondary"
            ++ (
              switch state {
              | Selected(_file) => ""
              | _ => " pure-button-disabled"
              }
            )
          )
          onClick=(_evt => send(FetchUrl))>
          <i className="fa fa-upload" />
          <span> (str("Upload File")) </span>
        </div>
      </div>
      (
        switch state {
        | Uploading({file: {size}, uploaded}) =>
          <progress
            value=(uploaded |> string_of_int)
            max=(size |> string_of_int)
          />
        | _ => ReasonReact.nullElement
        }
      )
    </div>
};
