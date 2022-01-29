#include <stdio.h>
#include <unistd.h>
#include <md4c.h>
#include <md4c-html.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/***************************
 * Defines
 * *************************/

#define STRINGIFY(x) #x
#define VERIFY_PTR(ptr) if (NULL == ptr) { die(1, "[%s; %s(..) at line %d] Verification of the pointer '%s' failed due to it being NULL.\n", __FILE__, __func__, __LINE__, STRINGIFY(ptr)); }
#define FILES_PER_DIR_MAX 256
#define UNUSED(x) (void)x;

__attribute__((__format__ (gnu_printf, 2, 3)))
void die(int code, char* format, ...);
int get_dirs(char* path, char* out[FILES_PER_DIR_MAX]);
int walk_dir_for_md_files(char* dir);
int file_exists(char* path);
int create_neccessary_files();
int read_file(char* path, char** buffer);
char* concat_strings(char* str1, char* str2);
char** get_markdown_dirs();



/***************************
 * Growable Buffer
 * (Required for logic with md4c)
 * *************************/


struct growable_buffer {
    char* data;
    size_t cap;
    size_t cur_size;
};


static void growable_buffer_init(struct growable_buffer* buf, size_t capacity) {
    buf->cur_size = 0;
    buf->cap = capacity;
    buf->data = (char*) malloc(buf->cap);
    VERIFY_PTR(buf->data);
}

static void growable_buffer_finish(struct growable_buffer* buf) {
    if (buf->data)
        free(buf->data);
}

static void growable_buffer_grow(struct growable_buffer* buf, size_t new_capacity) {
    buf->data = realloc(buf->data, new_capacity);
    buf->cap = new_capacity;
    VERIFY_PTR(buf->data);
}

static void growable_buffer_push(struct growable_buffer* buf, const char* data, size_t data_size) {
    if (buf->cap < buf->cur_size + data_size)
        growable_buffer_grow(buf, (buf->cur_size + buf->cur_size / 2 + data_size));
    
    memcpy(buf->data + buf->cur_size, data, data_size);
    buf->cur_size += data_size;
}


/***************************
 * Required by MD4C
 * *************************/
// Userdata will always be a growable_buffer*
static void process_output(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    growable_buffer_push((struct growable_buffer*) userdata, text, size);
}

/***************************
 * Program
 * *************************/

char* concat_strings(char* str1, char* str2) {
    char * s = (char*) malloc(snprintf(NULL, 0, "%s %s", str1, str2) + 1);
    sprintf(s, "%s%s", str1, str2);
    return s;
}

__attribute__((__format__ (gnu_printf, 2, 3)))
void die(int code, char* format, ...) {
    va_list args;
    va_start(args, format);

    if (code == 0) {
        fprintf(stdout, "[SUCCESS]\n");
        vfprintf(stdout, format, args);
        fprintf(stdout, "Exiting with code 0...\n");
        goto out;
    } else {
        fprintf(stderr, "[ERROR]\n");
        vfprintf(stderr, format, args);
        fprintf(stderr, "Exiting with error code %d...\n", code);
        goto out;
    }

out:
    va_end(args);
    exit(code);
}

int read_file(char* path, char** buffer) {
    FILE* fp = fopen(path, "r");
    VERIFY_PTR(fp)
    fseek (fp, 0, SEEK_END);
    int length = ftell (fp);
    fseek (fp, 0, SEEK_SET);
    *buffer = malloc (length);
    VERIFY_PTR(*buffer)
    if (*buffer)
    {
        fread (*buffer, 1, length, fp);
    }
    fclose (fp);
    return length;
}


int file_exists(char* path) {
    return access(path, F_OK);
}

int create_neccessary_files() {
    if (file_exists("html")) {
        mkdir("html", 0755);
    } else {
        rmdir("html");
        mkdir("html", 0755);
    }

//    if (file_exists("content")) {
//        die(1, "Did not find content folder\n");
//    }
    return 0;
}



int get_files(const char* path, char*** files) {
    if (*files == NULL) {
        *files = calloc(FILES_PER_DIR_MAX, sizeof(char**));
    }
    DIR* content_dir = opendir(path);

    VERIFY_PTR(content_dir)
    int file_count = 0;
    struct dirent* entry;
    while ((entry = readdir(content_dir))) {
        if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0) {
            continue;
        }

        (*files)[file_count] = malloc(strlen(entry->d_name) + 1);
        strcpy((*files)[file_count], entry->d_name);
        file_count++;
    }
    closedir(content_dir);
    return file_count;
}

int walk_dir_for_md_files(char* dir) {
    int status = chdir("content");
    if (status != 0) {
        die(status, "Failed to change into 'content' directory: %s\n", strerror(status));
    }

    char** md_files = calloc(FILES_PER_DIR_MAX, sizeof(char*) + 1);
    int file_count = get_files(dir, &md_files);
    UNUSED(file_count)

    int status_two = chdir(dir);
    if (status_two != 0) {
        die(status, "Failed to change into '%s' directory: %s\n", dir, strerror(status));
    }

    char* html_post_header = NULL;
    int header_length = read_file("../../header.html", &html_post_header);
    UNUSED(header_length)
    
    char* html_post_footer = NULL;
    int footer_length = read_file("../../footer.html", &html_post_footer);
    VERIFY_PTR(html_post_footer);
    UNUSED(footer_length)

    for (int i = 0; i < file_count; i++) {
        fprintf(stdout, "Attempting to convert blog entry %s...\n", md_files[i]);
        // WRITE HEADER
        char* final_name = concat_strings(concat_strings("../../html/", concat_strings(dir, "/")), concat_strings(md_files[i], ".html"));
        FILE* out_file = fopen(final_name, "wb");
        VERIFY_PTR(out_file);
        fputs(html_post_header, out_file);

        // GET MD FILE CONTENTS
        char* md_file_buffer = NULL;
        int md_file_len = read_file(md_files[i], &md_file_buffer);
        VERIFY_PTR(md_file_buffer);

        // WRITE MARKDOWN
        struct growable_buffer out_buf = {0};
        growable_buffer_init(&out_buf, md_file_len);

        int status = md_html(
            md_file_buffer,
            (MD_SIZE)md_file_len,
            process_output,
            (void*) &out_buf,
            MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_UNDERLINE | MD_FLAG_LATEXMATHSPANS | MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_TASKLISTS,
            MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_UNDERLINE | MD_FLAG_LATEXMATHSPANS | MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_TASKLISTS
        );

        if (status != 0) {
            die(status, "MD4C-Error (Handled in Program): Parsing of %s failed with code %d", md_files[i], status);
        }

        VERIFY_PTR(out_buf.data);
        fputs(out_buf.data, out_file);
        growable_buffer_finish(&out_buf);    
        // WRITE FOOTER
        fputs(html_post_footer, out_file);
        printf("Converted blog entry %s\n", md_files[i]);
        fclose(out_file);
    }

    chdir("..");
    chdir("..");
    return 0;
}



int main() {
    create_neccessary_files();
    char** dirs = calloc(FILES_PER_DIR_MAX, sizeof(char*) + 1);
    // We trust that everything in 'content' is a directory
    int num_dirs = get_files("content", &dirs);
    VERIFY_PTR(dirs)

    for (int i = 0; i < num_dirs; i++) {
        walk_dir_for_md_files(dirs[i]);
    }

    return 0;
}