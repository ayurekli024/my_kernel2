#include "ardaos.h"
#include "libc.h"

// {1} ile .data segmentine alınır: ELF loader tüm 4096 baytı kesin olarak map eder,
// stack kullanmaz ve çalışma zamanında PTR() ile gerçek kullanıcı adresine çevrilir.
static char safe_text_buffer[4096] = {1};

typedef struct {
    char* text_buffer;
    int cursor_pos;
    int text_len;
    char current_file[16];
} editor_state_t;

void update_display(editor_state_t* state) {
    sys_set_window_text(state->text_buffer);
}

void load_file(editor_state_t* state, const char* filename) {
    int fd = sys_open(filename, "");
    if (fd >= 0) {
        int bytes = sys_read(fd, (unsigned char*)state->text_buffer, 4095);
        if (bytes > 0) {
            state->text_buffer[bytes] = '\0';
            state->text_len = bytes;
            state->cursor_pos = bytes;
        }
        sys_close(fd);
    }
    update_display(state);
}

void save_file(editor_state_t* state) {
    const char* filename = state->current_file;
    if (!filename || filename[0] == '\0' || filename[0] == ' ') return;

    char name[9];
    char ext[4];
    for (int k = 0; k < 8; k++) name[k] = ' ';
    name[8] = '\0';
    for (int k = 0; k < 3; k++) ext[k] = ' ';
    ext[3] = '\0';

    int i = 0, n = 0, e = 0;
    while (filename[i] && filename[i] != '.' && n < 8) {
        char c = filename[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        name[n++] = c;
    }
    if (filename[i] == '.') {
        i++;
        while (filename[i] && e < 3) {
            char c = filename[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            ext[e++] = c;
        }
    }
    if (name[0] == ' ') return;

    sys_write_file(name, ext, (unsigned char*)state->text_buffer);
}

void _start(const char* args) {
    editor_state_t state;

    state.text_buffer = safe_text_buffer;
    state.cursor_pos = 0;
    state.text_len = 0;
    state.text_buffer[0] = '\0';

    sys_create_window("ArdaPad", 400, 300);
    sys_create_button(20, 250, 80, 25, "Kaydet");

    if (args && args[0] != '\0' && args[0] != ' ') {
        int i = 0;
        while (args[i] && i < 15) { state.current_file[i] = args[i]; i++; }
        state.current_file[i] = '\0';
    } else {
        strcpy(state.current_file, "YENI.TXT");
    }

    load_file(&state, state.current_file);

    while (1) {
        int ev_id;
        int ev_type = sys_poll_event(&ev_id);

        if (ev_type == 1 && ev_id == 0) {
            save_file(&state);
        }

        char key = sys_get_key();
        if (key) {
            if (key == '\b' && state.cursor_pos > 0) {
                for (int i = state.cursor_pos - 1; i < state.text_len; i++)
                    state.text_buffer[i] = state.text_buffer[i + 1];
                state.cursor_pos--;
                state.text_len--;
            }
            else if (key >= 32 && key <= 126 && state.text_len < 4095) {
                for (int i = state.text_len; i > state.cursor_pos; i--)
                    state.text_buffer[i] = state.text_buffer[i - 1];
                state.text_buffer[state.cursor_pos++] = key;
                state.text_buffer[++state.text_len] = '\0';
            }
            else if (key == '\n' && state.text_len < 4095) {
                for (int i = state.text_len; i > state.cursor_pos; i--)
                    state.text_buffer[i] = state.text_buffer[i - 1];
                state.text_buffer[state.cursor_pos++] = '\n';
                state.text_buffer[++state.text_len] = '\0';
            }
            update_display(&state);
        }
        sys_sleep(10);
    }
}