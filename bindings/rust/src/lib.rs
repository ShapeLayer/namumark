use libc::{c_char, c_int, size_t};
use std::ffi::{CStr, CString};
use std::ptr;
use std::slice;

pub const NAMUMARK_OUTPUT_HTML: c_int = 0;
pub const NAMUMARK_OUTPUT_AST_JSON: c_int = 1;
pub const NAMUMARK_OK: c_int = 0;

#[repr(C)]
struct NamumarkBuffer {
    data: *mut c_char,
    size: size_t,
}

#[link(name = "namumark")]
extern "C" {
    fn namumark_render(
        input: *const c_char,
        input_size: size_t,
        format: c_int,
        output: *mut NamumarkBuffer,
    ) -> c_int;
    fn namumark_buffer_free(buffer: *mut NamumarkBuffer);
    fn namumark_status_message(status: c_int) -> *const c_char;
}

#[derive(Debug)]
pub struct NamumarkError {
    pub status: c_int,
    pub message: String,
}

impl std::fmt::Display for NamumarkError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for NamumarkError {}

fn status_message(status: c_int) -> String {
    unsafe {
        let ptr = namumark_status_message(status);
        if ptr.is_null() {
            return "unknown error".to_string();
        }
        CStr::from_ptr(ptr).to_string_lossy().into_owned()
    }
}

pub fn render(input: &str, output_format: c_int) -> Result<String, NamumarkError> {
    let c_input = CString::new(input).map_err(|_| NamumarkError {
        status: -1,
        message: "input contains NUL byte".to_string(),
    })?;
    let mut output = NamumarkBuffer {
        data: ptr::null_mut(),
        size: 0,
    };

    let status = unsafe {
        namumark_render(
            c_input.as_ptr(),
            input.as_bytes().len(),
            output_format,
            &mut output,
        )
    };
    if status != NAMUMARK_OK {
        return Err(NamumarkError {
            status,
            message: status_message(status),
        });
    }

    let bytes = unsafe { slice::from_raw_parts(output.data as *const u8, output.size) };
    let rendered = String::from_utf8_lossy(bytes).into_owned();
    unsafe { namumark_buffer_free(&mut output) };
    Ok(rendered)
}

pub fn render_html(input: &str) -> Result<String, NamumarkError> {
    render(input, NAMUMARK_OUTPUT_HTML)
}

pub fn render_ast_json(input: &str) -> Result<String, NamumarkError> {
    render(input, NAMUMARK_OUTPUT_AST_JSON)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn renders_html() {
        let html = render_html("== 제목 ==\n본문 [[문서|링크]]\n").unwrap();
        assert!(html.contains("<h2>제목</h2>"));
        assert!(html.contains("<a href=\"문서\">링크</a>"));
    }
}
