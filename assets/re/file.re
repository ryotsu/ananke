type file = Fetch.blob;

[@bs.get] external get_files : Dom.element => array(file) = "files";

[@bs.get] external get_size : Fetch.blob => int = "size";

[@bs.get] external get_name : file => string = "name";

[@bs.send] external _slice : (Fetch.blob, int, int) => Fetch.blob = "slice";

let slice = (start, end', blob) => _slice(blob, start, end');
