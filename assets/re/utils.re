type file_ = Fetch.blob;

type file = {
  file: file_,
  name: string,
  size: int
};

let map = f =>
  fun
  | Some(v) => Some(f(v))
  | None => None;

let unwrap =
  fun
  | Some(v) => v
  | None => raise(Invalid_argument("Passed `None` to unwrap"));

let str = ReasonReact.stringToElement;

[@bs.get] external get_file_size : file_ => int = "size";

[@bs.get] external get_file_name : file_ => string = "name";

[@bs.send] external _slice : (Fetch.blob, int, int) => Fetch.blob = "slice";

let slice = (start, end', blob) => _slice(blob, start, end');
