open Utils;

type action =
  | Select(file)
  | FetchUrl
  | Upload(string)
  | ContinueUpload(string, int);

type upload = {
  file,
  url: string,
  uploaded: int
};

type state =
  | NotSelected
  | Selected(file)
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
  Fetch.Response.headers(resp)
  |> Fetch.Headers.get(header)
  |> map(Js.Promise.resolve)
  |> unwrap;

let make_init_req_body = (name, size) =>
  Json.Encode.(object_([("name", string(name)), ("size", int(size))]))
  |> Json.stringify
  |> Fetch.BodyInit.make;

let fetch_url = (name, size, set_location) => {
  let body = make_init_req_body(name, size);
  let headers = Fetch.HeadersInit.make({"content-type": "application/json"});
  Js.Promise.(
    Fetch.fetchWithInit(
      "/share",
      Fetch.RequestInit.make(~method_=Post, ~body, ~headers, ())
    )
    |> then_(get_header_value("location"))
    |> then_(location => {
         set_location(location);
         resolve();
       })
    |> ignore
  );
};

let upload_blob = (file, size, start, end', url, continue) => {
  let end' = size > end' ? end' : size;
  let body = file |> slice(start, end') |> Fetch.BodyInit.makeWithBlob;
  Js.Promise.(
    Fetch.fetchWithInit(url, Fetch.RequestInit.make(~method_=Put, ~body, ()))
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
    | (FetchUrl, Selected({name, size} as file)) =>
      ReasonReact.UpdateWithSideEffects(
        InitiatedUpload(file),
        (self => fetch_url(name, size, url => self.send(Upload(url))))
      )
    | (Upload(url), InitiatedUpload(file)) =>
      ReasonReact.UpdateWithSideEffects(
        Uploading({file, url, uploaded: 0}),
        (
          self =>
            upload_blob(file.file, file.size, 0, offset, url, (url, end') =>
              self.send(ContinueUpload(url, end'))
            )
        )
      )
    | (ContinueUpload(url, up), Uploading({file: {file, size}} as upload)) =>
      up == size ?
        ReasonReact.Update(Uploaded({...upload, uploaded: up})) :
        ReasonReact.UpdateWithSideEffects(
          Uploading({...upload, uploaded: up}),
          (
            self =>
              upload_blob(file, size, up, up + offset, url, (url, end') =>
                self.send(ContinueUpload(url, end'))
              )
          )
        )
    | (_, _) => ReasonReact.NoUpdate
    },
  render: ({state, send}) =>
    <div className="centered">
      (
        switch state {
        | Uploading({url}) => <span> (str(url)) </span>
        | Uploaded({url}) => <span> (str(url)) </span>
        | _ => ReasonReact.nullElement
        }
      )
      <div className="box">
        <div
          className=(
            "file-upload pure-button pure-button-primary"
            ++ (
              switch state {
              | InitiatedUpload(_file) => " pure-button-disabled"
              | Uploading(_upload) => " pure-button-disabled"
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
