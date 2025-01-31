#include "uchat.h"

static int save_file_in_db(t_server_info *info, json_object *obj,
                           t_file_list *file_list) {
	char command[1024];
    int user_id = mx_js_g_int(mx_js_o_o_get(obj, "user_id"));
    int room_id = mx_js_g_int(mx_js_o_o_get(obj, "room_id"));
    
    command[sprintf(command, "INSERT INTO msg_history (user_id, room_id, \
                    message, addition_cont)VALUES ('%d', '%d', '%s', 'file'); \
                    SELECT last_insert_rowid()",
                    user_id, room_id, file_list->file_name)] = '\0';
    if (sqlite3_exec(info->db, command, mx_get_data, obj, NULL) != SQLITE_OK)
		return -1;
	return 1;
}

static void send_notification(t_server_info *info, t_socket_list *csl,
                              t_file_list *file_list) {
    json_object *send_obj = mx_create_basic_json_object(MX_MSG_TYPE);

    mx_js_o_o_add(send_obj, "user_id", mx_js_n_int(mx_js_g_int(mx_js_o_o_get(csl->obj, "user_id"))));
    mx_js_o_o_add(send_obj, "room_id", mx_js_n_int(mx_js_g_int(mx_js_o_o_get(csl->obj, "room_id"))));
    mx_js_o_o_add(send_obj, "login", mx_js_n_str(mx_js_g_str(mx_js_o_o_get(csl->obj, "login"))));
    if (mx_detect_file_extention(file_list->file_name) == 1)
        mx_js_o_o_add(send_obj, "add_info", mx_js_n_int(2));
    else if (mx_detect_file_extention(file_list->file_name) == 2)
        mx_js_o_o_add(send_obj, "add_info", mx_js_n_int(4));
    else
        mx_js_o_o_add(send_obj, "add_info", mx_js_n_int(1));
    mx_js_o_o_add(send_obj, "data", mx_js_n_str(file_list->file_name));
    mx_js_o_o_add(send_obj, "id", mx_js_n_int(mx_js_g_int(mx_js_o_o_get(csl->obj, "id"))));
    mx_send_json_to_all_in_room(info, send_obj);
    json_object_put(send_obj);
}

static void set_file_name(json_object *obj) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    const char *file_name;
    char *new_name;

    file_name = mx_js_g_str(mx_js_o_o_get(obj,
                                                              "file_name"));
    new_name = mx_strnew(strlen(file_name) + 60);
    sprintf(new_name, "%d_%02d_%02d_%02d_%02d_%02d_%s",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, file_name);
    json_object_set_string(mx_js_o_o_get(obj, "file_name"), new_name);
    mx_strdel(&new_name);
}

int mx_add_new_file_server(t_file_list **input_files, t_socket_list *csl) {
    int file_size = mx_js_g_int(mx_js_o_o_get(csl->obj, "file_size"));

    if (file_size > 0 && file_size <= MX_MAX_FILE_SIZE) {
        t_file_list *new_file_list = mx_new_file_list_elem(csl->obj);

        if (new_file_list != NULL) {
            mx_push_file_elem_to_list(input_files, new_file_list);
            return 0;
        }
    }
    else
        fprintf(stderr, "Bad file size: %d\n", file_size);
    return 1;
}

int mx_add_data_to_file_server(t_file_list **input_files, json_object *obj) {
    t_file_list *tmp = *input_files;
    int user_id = mx_js_g_int(mx_js_o_o_get(obj, "user_id"));

    while(tmp && tmp->id != user_id)
        tmp = tmp->next;
    if (tmp) {
        fwrite(mx_js_g_str(mx_js_o_o_get(obj, "data")), 1,
               mx_js_g_str_len(mx_js_o_o_get(obj,
                                          "data")), tmp->file);
        return 0;
    }
    else
        return 1;
}

int mx_final_file_input_server(t_server_info *info, t_socket_list *csl) {
    t_file_list *file_list = info->input_files;
    t_file_list *prev_elem = NULL;
    int user_id = mx_js_g_int(mx_js_o_o_get(csl->obj, "user_id"));

    while(file_list && file_list->id != user_id) {
        prev_elem = file_list;
        file_list = file_list->next;
    }
    if (file_list) {
        int final_size = ftell(file_list->file);

        fclose(file_list->file);
        if (final_size != file_list->file_size) {
            char *full_file_name = mx_strjoin(MX_SAVE_FOLDER_IN_SERVER, file_list->file_name);

            fprintf(stderr, "File size error: %d|%d\n", final_size, file_list->file_size);
            remove(full_file_name);
            mx_strdel(&full_file_name);
        }
        else {
            if (save_file_in_db(info, csl->obj, file_list) != -1)
                send_notification(info, csl, file_list);
        }
        if (prev_elem == NULL)
            info->input_files = file_list->next;
        else
            prev_elem->next = prev_elem->next->next;
        free(file_list);
    }
    return 0;
}

int mx_save_file_in_server(t_server_info *info, t_socket_list *csl) {
    int piece = mx_js_g_int(mx_js_o_o_get(csl->obj, "piece"));

    if (piece == 1) {
        set_file_name(csl->obj);
        mx_add_new_file_server(&(info->input_files), csl);
    }
    else if (piece == 2) {
        mx_add_data_to_file_server(&(info->input_files), csl->obj);
    }
    else if (piece == 3) {
        mx_add_data_to_file_server(&(info->input_files), csl->obj);
        mx_final_file_input_server(info, csl);
    }
    else
        fprintf(stderr, "piece from client is wrong for files: %d\n", piece);
    return 0;
}
