#include <glib.h>
#include <locale.h>
#include <string.h>
typedef enum {
           OREZ_DOC_FRAG,
        OREZ_CODE_FRAG,
        OREZ_CF_NAME,
        OREZ_CF_LABEL_REF,
        OREZ_CF_LANG_MARK,
        OREZ_CF_OPERATOR,
        OREZ_CF_LABEL,
        OREZ_CF_BODY,
        OREZ_CF_SNIPPET,
        OREZ_CF_REF,
        OREZ_CF_INDENT,
        OREZ_ELEMENT_TYPE_PLACEHOLDER
} OrezElementType;
typedef struct {
        OrezElementType type;
        gsize line_num;
        GString *content;
} OrezElement;
typedef struct {
        GPtrArray *doc_order;
        GPtrArray *prog_order;
        GPtrArray *emissions;
} OrezTie;
const gchar *utf8_hole = "@";
const gchar *utf8_grid = "#";
const gchar *utf8_line_cont = "\\";
const gchar *utf8_lt = "<";
const gchar *utf8_gt = ">";
const gchar *utf8_left_bracket = "[";
const gchar *utf8_right_bracket = "]";
const gchar *utf8_plus = "+";
const gchar *utf8_up = "^";
const gchar *utf8_linebreak="\n";
gunichar ucs_hole = 64;
gunichar ucs_grid = 35;
gunichar ucs_line_cont = 92;
gunichar ucs_lt = 60;
gunichar ucs_gt = 62;
gunichar ucs_left_bracket = 91;
gunichar ucs_right_bracket = 93;
gunichar ucs_plus = 43;
gunichar ucs_up = 94;
gunichar ucs_linebreak = 10;
gboolean orez_show_line_num = FALSE;
const gchar *yaml_indent = "    ";
gboolean orez_tangle_mode = FALSE;
gboolean orez_weave_mode = FALSE;
gchar *orez_entrance = NULL;
gchar *orez_output = NULL;
gchar *orez_separator = NULL;
static GOptionEntry orez_entries[] = {
        {"tangle", 't', 0, G_OPTION_ARG_NONE, &orez_tangle_mode,
         "Turn on tangle mode.", NULL},
        {"line", 'l', 0, G_OPTION_ARG_NONE, &orez_show_line_num,
         "Show line number in the output of tangle mode.", NULL},
        {"weave", 'w', 0, G_OPTION_ARG_NONE, &orez_weave_mode,
         "Turn on weave mode.", NULL},
        {"entrance", 'e', 0, G_OPTION_ARG_STRING, &orez_entrance,
         "Set <code fragment name> as the entrance for tangling code.",
         "<code fragment name>"},
        {"output", 'o', 0, G_OPTION_ARG_STRING, &orez_output,
         "Send output into <file>.", "<file>"},
        {"separator", 's', 0, G_OPTION_ARG_STRING, &orez_separator,
         "Set the separator for multi-entrances and outputs.",
         "<character>"},
        {NULL}
};
#define FILE_READ(input, c) g_io_channel_read_unichar(input, c, NULL)
#define TEXT_HEAD(text) ((text)->str)
#define TEXT_TAIL(text) g_utf8_offset_to_pointer((text)->str, \
                                                  g_utf8_strlen((text)->str, -1))
#define TEXT_READ(cursor, c) do {                  \
                c = g_utf8_get_char(cursor);       \
                cursor = g_utf8_next_char(cursor); \
        } while (0)
#define EVERY_BRANCH(tree, it) \
        for (GNode *it = g_node_first_child(tree); \
             it != NULL; \
             it = g_node_next_sibling(it))
#define FILE_WRITE_TEXT(output, text) do \
{ \
        GIOStatus s = g_io_channel_write_chars(output, \
                                               text->str, \
                                               -1, \
                                               NULL, \
                                               NULL); \
        if (s == G_IO_STATUS_ERROR) g_error("FILE_WRITE_TEXT error!"); \
} while (0)
#define FILE_WRITE(output, c) do \
{ \
        GIOStatus s = g_io_channel_write_unichar(output, c, NULL); \
        if (s == G_IO_STATUS_ERROR) g_error("FILE_WRITE_TEXT error!"); \
} while (0)
static gboolean char_belong_to(gunichar c, const gchar *charset)
{
        gboolean ret = FALSE;
        for (; *charset != '\0'; charset = g_utf8_next_char(charset)) {
                if (c == g_utf8_get_char(charset)) {
                        ret = TRUE;
                        break;
                }
        }
        return ret;
}
static gboolean last_line_is_white(gchar *h, gchar *t)
{
        gboolean ret = TRUE;
        gunichar c;
        while (t != h) {
                t = g_utf8_prev_char(t);
                c = g_utf8_get_char(t);
                if (!char_belong_to(c, " \t")) {
                        if (c != ucs_linebreak) ret = FALSE;
                        else break;
                }
        }
        return ret;
}
static gboolean is_hole(gunichar c, OrezElement *last)
{
        if (c != ucs_hole) return FALSE;
        else {
                if (!last) return TRUE;
                else {
                        gchar *h = TEXT_HEAD(last->content);
                        gchar *t = TEXT_TAIL(last->content);
                        if (last_line_is_white(h, t)) return TRUE;
                        else return FALSE;
                }
        }
}
static OrezElementType infer_type_of_next_element(GIOChannel *input,
                                                  GString *cache,
                                                  gsize *line_num)
{
        enum { INIT, MAYBE_L_C } state = INIT;
        gunichar c; GIOStatus status;
        while ((status = FILE_READ(input, &c)) == G_IO_STATUS_NORMAL) {
                if (c == ucs_linebreak) (*line_num)++;
                g_string_append_unichar(cache, c);
                switch (state) {
                case INIT:
                        if (c == ucs_grid) return OREZ_CODE_FRAG;
                        else if (c == ucs_line_cont) state = MAYBE_L_C;
                        else if (c == ucs_linebreak) return OREZ_DOC_FRAG;
                        else ;
                        break;
                case MAYBE_L_C:
                        if (c == ucs_grid) return OREZ_CODE_FRAG;
                        else if (c == ucs_linebreak) state = INIT;
                        else if (char_belong_to(c, " \t")) ;
                        else return OREZ_DOC_FRAG;
                        break;
                default:
                        g_error("type_of_next_element: Illegal state!");
                }
        }
        /* If the above procedure fails, return the default type */
        return OREZ_DOC_FRAG;
}
static GNode *stage_1st(gchar *file_name)
{
        GNode *root = g_node_new(file_name);
              GIOChannel *input = g_io_channel_new_file(file_name, "r", NULL);
        if (!input) {
                g_error("Failed to open the input file %s to orez!", file_name);
        }
          gsize line_count = 1; OrezElement *last = NULL;
        gunichar c; GIOStatus status;
        last = g_slice_new(OrezElement);
        last->type = OREZ_DOC_FRAG;
        last->line_num = line_count;
        last->content = g_string_new(NULL);
        g_node_append_data(root, last);
        while ((status = FILE_READ(input, &c)) == G_IO_STATUS_NORMAL) {
                if (c == ucs_linebreak) line_count++;
                if (!is_hole(c, last)) {
                        g_string_append_unichar(last->content, c);
                } else {
                        gsize line_num_bak = line_count;
                        GString *cache = g_string_new(NULL);
                        OrezElementType x = infer_type_of_next_element(input, cache, &line_count);
                                                last = g_slice_new(OrezElement);
                        last->type = x;
                        last->line_num = line_num_bak;
                        last->content = cache;
                        g_node_append_data(root, last);
                }
        }
                if (last->content && last->content->len > 0) {
                gchar *end = g_utf8_prev_char(TEXT_TAIL(last->content));
                c = g_utf8_get_char(end);
                if (c != ucs_linebreak) g_string_append(last->content, utf8_linebreak);
        }
        g_io_channel_unref(input);
	return root;
}
static OrezElement *extract_code_frag_name(OrezElement *code_frag,
                                           gchar **cursor,
                                           gchar *end,
                                           gsize *line_offset)
{
        
        OrezElement *e = g_slice_new(OrezElement);
        e->type = OREZ_CF_NAME;
        e->line_num = code_frag->line_num + (*line_offset);
        e->content = g_string_new(NULL);
        
        gunichar c;
        while (*cursor != end) {
                TEXT_READ(*cursor, c);
                if (c == ucs_linebreak) (*line_offset)++;
                if (c == ucs_grid) break;
                g_string_append_unichar(e->content, c);
        }
        return e;
}
static OrezElement *extract_text_in_brackets(OrezElement *code_frag,
                                             gchar **cursor,
                                             gchar *end,
                                             gsize *line_offset,
                                             gunichar left_bracket,
                                             gunichar right_bracket,
                                             OrezElementType type)
{
        gchar *history = *cursor;
        GString *text = g_string_new(NULL);
        gunichar c;
        enum { INIT, BODY, FAILURE, SUCCESS } state = INIT;
        while (*cursor != end) {
                TEXT_READ(*cursor, c);
                switch (state) {
                case INIT:
                        if (char_belong_to(c, " \t")) ;
                        else if (c == left_bracket) state = BODY;
                        else state = FAILURE;
                        break;
                case BODY:
                        if (c == right_bracket) state = SUCCESS;
                        else if (c == ucs_linebreak) state = FAILURE;
                        else g_string_append_unichar(text, c);
                        break;
                default:
                        g_error("Line %zu error: "
                                "Illegal language mark or code fragment label!",
                                code_frag->line_num);
                }
                
                if (state == SUCCESS) break;
                else if (state == FAILURE) {
                        *cursor = history;
                        g_string_free(text, TRUE);
                        return NULL;
                }
        }
        
        OrezElement *e = g_slice_new(OrezElement);
        e->type = type;
        e->line_num = code_frag->line_num + (*line_offset);
        e->content = text;
        return e;
}
static void skip_spaces_and_tabs(gchar **cursor, gchar *end)
{
        gunichar c;
        gchar *t;
        while (*cursor != end) {
                t = g_utf8_next_char(*cursor);
                c = g_utf8_get_char(*cursor);
                if (char_belong_to(c, " \t")) *cursor = t;
                else break;
        }
}
static void parse_head_of_code_fragment(GNode *code_frag_node)
{
        OrezElement *e = code_frag_node->data;
        gchar *cursor = TEXT_HEAD(e->content);
        gchar *end = TEXT_TAIL(e->content);
        gsize line_offset = 0;
        OrezElement *name = extract_code_frag_name(e, &cursor, end, &line_offset);
        g_node_append_data(code_frag_node, name);
        skip_spaces_and_tabs(&cursor, end);
        if (g_utf8_get_char(cursor) == ucs_left_bracket) {
                OrezElement *mark = extract_text_in_brackets(e,
                                                             &cursor,
                                                             end,
                                                             &line_offset,
                                                             ucs_left_bracket,
                                                             ucs_right_bracket,
                                                             OREZ_CF_LANG_MARK);
                if (mark) g_node_append_data(code_frag_node, mark);
        }
        skip_spaces_and_tabs(&cursor, end);
        if (g_utf8_get_char(cursor) == ucs_lt) {
                OrezElement *target_label;
                target_label = extract_text_in_brackets(e,
                                                        &cursor,
                                                        end,
                                                        &line_offset,
                                                        ucs_lt,
                                                        ucs_gt,
                                                        OREZ_CF_LABEL_REF);
                if (target_label) g_node_append_data(code_frag_node, target_label);
        }
        skip_spaces_and_tabs(&cursor, end);
        if (g_utf8_get_char(cursor) == ucs_plus) {
                cursor = g_utf8_next_char(cursor);
                OrezElement *o_e = g_slice_new(OrezElement);
                o_e->type = OREZ_CF_OPERATOR;
                o_e->line_num = e->line_num + line_offset;
                o_e->content = g_string_new("+");
                g_node_append_data(code_frag_node, o_e);
        } else if (g_utf8_get_char(cursor) == ucs_up) {
                cursor = g_utf8_next_char(cursor);
                if (g_utf8_get_char(cursor) == ucs_plus) {
                        OrezElement *o_e = g_slice_new(OrezElement);
                        o_e->type = OREZ_CF_OPERATOR;
                        o_e->line_num = e->line_num + line_offset;
                        o_e->content = g_string_new("^+");
                        g_node_append_data(code_frag_node, o_e);
                        cursor = g_utf8_next_char(cursor);
                } else {
                        g_error("Line %zu error: "
                                "Illegal operator!", e->line_num + line_offset);
                }
        }
        skip_spaces_and_tabs(&cursor, end);
        if (g_utf8_get_char(cursor) == ucs_linebreak) {
                line_offset++;
                cursor = g_utf8_next_char(cursor);
                if (g_utf8_get_char(cursor) == ucs_lt) {
                        OrezElement *source_label;
                        source_label = extract_text_in_brackets(e,
                                                                &cursor,
                                                                end,
                                                                &line_offset,
                                                                ucs_lt,
                                                                ucs_gt,
                                                                OREZ_CF_LABEL);
                        if (source_label) g_node_append_data(code_frag_node, source_label);
                        skip_spaces_and_tabs(&cursor, end);
                        if (g_utf8_get_char(cursor) == ucs_linebreak) {
                                line_offset++;
                                cursor = g_utf8_next_char(cursor);
                        }
                }
                OrezElement *o_e = g_slice_new(OrezElement);
                o_e->type = OREZ_CF_BODY;
                o_e->line_num = e->line_num + line_offset;
                o_e->content = g_string_new(NULL);
                while (cursor != end) {
                        g_string_append_unichar(o_e->content,
                                                g_utf8_get_char(cursor));
                        cursor = g_utf8_next_char(cursor);
                }
                g_node_append_data(code_frag_node, o_e);
        }

        /* 
         * Now the content of e is useless because it has been broken
         * up into the elements above. It should be released.
         */
        g_string_free(e->content, TRUE);
        e->content = NULL;        
}
static GNode *stage_2nd(GNode *root)
{
        EVERY_BRANCH(root, it) {
                OrezElement *e = it->data; if (e->type != OREZ_CODE_FRAG) continue;
                parse_head_of_code_fragment(it);
        }
        return root;
}
static gboolean is_grid(gunichar c, OrezElement *last)
{
        if (c != ucs_grid) return FALSE;
        else {
                if (!last) return TRUE;
                else {
                        gchar *h = TEXT_HEAD(last->content);
                        gchar *t = TEXT_TAIL(last->content);
                        if (last_line_is_white(h, t)) return TRUE;
                        else return FALSE;
                }
        }
}
static gboolean is_code_frag_ref(gchar **cursor,
                                 gchar *end,
                                 GString *cache,
                                 gsize *line_offset)
{
        gboolean result = FALSE;
        enum { INIT, MAYBE_L_C, MAYBE_REF, FAILURE, SUCCESS } state = INIT;
        GString *o_cache = g_string_new(NULL);
        while (*cursor != end) {
                gunichar c; TEXT_READ(*cursor, c);
                if (c == ucs_linebreak) (*line_offset)++;
                switch (state) {
                case INIT:
                        if (c == ucs_hole) {
                                g_string_append_unichar(o_cache, c);
                                state = MAYBE_REF;
                        } else {
                                g_string_append_unichar(cache, c);
                                if (c == ucs_line_cont) state = MAYBE_L_C;
                                else if (c == ucs_linebreak) state = FAILURE;
                                else ;
                        }
                        break;
                case MAYBE_L_C:
                        if (c == ucs_hole) {
                                g_string_append_unichar(o_cache, c);
                                state = MAYBE_REF;
                        } else if (c == ucs_linebreak) {
                                g_string_append_unichar(cache, c);
                                state = INIT;
                        } else if (char_belong_to(c, " \t")) {
                                g_string_append_unichar(cache, c);
                        } else state = FAILURE;
                        break;
                case MAYBE_REF:
                        g_string_append_unichar(o_cache, c);
                        if (char_belong_to(c, " \t")) ;
                        else if (c == ucs_linebreak) state = SUCCESS;
                        else state = FAILURE;
                        break;
                default:
                        g_error("is_code_frag_ref: Illegal state!");
                }
                if (state == FAILURE) {
                        g_string_append(cache, o_cache->str);
                        break;
                } 
                if (state == SUCCESS) {
                        result = TRUE;
                        break;
                }
        }
        if (state == MAYBE_REF) result = TRUE;
        g_string_free(o_cache, TRUE);
        return result;
}
GString *cut_out_indent(GString *text)
{
        if (text->len == 0) return text;
        GString *indent = g_string_new(NULL);
        gchar *h = TEXT_HEAD(text);
        gchar *t = g_utf8_prev_char(TEXT_TAIL(text));
        while (1) {
                gunichar c = g_utf8_get_char(t);
                if (c == ucs_linebreak) {
                        t = g_utf8_next_char(t);
                        g_string_truncate(text, text->len - strlen(t));
                        break;
                } else {
                        g_string_append_unichar(indent, c);
                        t = g_utf8_prev_char(t);
                }
        }
        return indent;
}
static void parse_code_frag_body(GNode *body)
{
        OrezElement *e = body->data;
        gchar *cursor = TEXT_HEAD(e->content);
        gchar *end = TEXT_TAIL(e->content);
        gsize line_offset = 0;
        OrezElement *last = NULL;
        while (cursor != end) {
                gunichar c = g_utf8_get_char(cursor);
                cursor = g_utf8_next_char(cursor);
                if (c == ucs_linebreak) line_offset++;
                if (!last || last->type == OREZ_CF_REF) {
                        last = g_slice_new(OrezElement);
                        last->type = OREZ_CF_SNIPPET;
                        last->line_num = e->line_num + line_offset;
                        last->content = g_string_new(NULL);
                        g_node_append_data(body, last);
                }
                if (!is_grid(c, last)) {
                        g_string_append_unichar(last->content, c);
                } else {
                        GString *cache = g_string_new(NULL);
                        gsize line_offset_bak = line_offset;
                        if (is_code_frag_ref(&cursor, 
                                             end,
                                             cache,
                                             &line_offset)) {
                                GString *indent = cut_out_indent(last->content);
                                if (indent == last->content) {
                                        last->type = OREZ_CF_INDENT;
                                } else {
                                        last = g_slice_new(OrezElement);
                                        last->type = OREZ_CF_INDENT;
                                        last->line_num = e->line_num + line_offset_bak;
                                        last->content = indent;
                                        g_node_append_data(body, last);
                                }
                                                                last = g_slice_new(OrezElement);
                                last->type = OREZ_CF_REF;
                                last->line_num = e->line_num + line_offset_bak;
                                last->content = cache;
                                g_node_append_data(body, last);
                        } else {
                                g_string_append_unichar(last->content, c);
                                g_string_append(last->content, cache->str);
                                g_string_free(cache, TRUE);
                        }
                }
        }
        g_string_free(e->content, TRUE);
        e->content = NULL;
}
static GNode *stage_3rd(GNode *root)
{
        EVERY_BRANCH(root, it) {
                OrezElement *e = it->data;
                if (e->type != OREZ_CODE_FRAG) continue;
                EVERY_BRANCH(it, o_it) {
                        OrezElement *o_e = o_it->data;
                        if (o_e->type != OREZ_CF_BODY) continue;
                        parse_code_frag_body(o_it);
                }
        }
        return root;
}
static void destroy_syntax_elements(GNode *root)
{
        EVERY_BRANCH(root, it) {
                OrezElement *e = it->data;
                if (e->content) g_string_free(e->content, TRUE);
                g_slice_free(OrezElement, e);
                destroy_syntax_elements(it);
        }
}
static GString *text_compact(GString *code_frag_name)
{
        GString *key = g_string_new(NULL);
        gchar *pos = TEXT_HEAD(code_frag_name);
        gchar *end = TEXT_TAIL(code_frag_name);
        gunichar c;
        while (pos != end) {
                TEXT_READ(pos, c);
                if (!char_belong_to(c, " \t\n\\")) {
                        g_string_append_unichar(key, c);
                }
        }        
        return key;
}
static void hash_table_wrap(GHashTable *table, GNode *cf_node)
{
        GString *name = NULL;
        EVERY_BRANCH(cf_node, it) {
                OrezElement *e = it->data;
                if (e->type == OREZ_CF_NAME) {
                        name = e->content;
                        break;
                }
        }
        GString *key = text_compact(name);
        OrezTie *tie = g_hash_table_lookup(table, key);
        if (!tie) {
                tie = g_slice_new(OrezTie);
                tie->doc_order = g_ptr_array_new();
                tie->prog_order = g_ptr_array_new();
                tie->emissions = g_ptr_array_new();
                g_ptr_array_add(tie->doc_order, cf_node);
                g_ptr_array_add(tie->prog_order, cf_node);
                g_hash_table_insert(table, key, tie);
                return;
        }
        const gchar *appending = utf8_plus;
        GString *target_label = NULL;
        GString *operator = NULL;
        EVERY_BRANCH(cf_node, it) {
                OrezElement *e = it->data;
                if (e->type == OREZ_CF_LABEL_REF) target_label = e->content;
                else if (e->type == OREZ_CF_OPERATOR) {
                        operator = e->content;
                        break;
                }
        }
        g_ptr_array_add(tie->doc_order, cf_node);
        if (!target_label) {
                if (!operator) {
                        OrezElement *e = cf_node->data;
                        g_error("Line %zu: This code fragment needs an operator",
                                e->line_num);
                } else if (g_str_equal(operator->str, utf8_plus)) {
                        g_ptr_array_add(tie->prog_order, cf_node);
                } else {
                        g_ptr_array_insert(tie->prog_order, 0, cf_node);
                }
        } else {
                GString *t = text_compact(target_label);
                gint id = -1;
                for (guint i = 0; i < tie->prog_order->len; i++) {
                        GNode *node_i = g_ptr_array_index(tie->prog_order,i);
                        EVERY_BRANCH(node_i, it) {
                                OrezElement *e = it->data;
                                if (e->type == OREZ_CF_LABEL) {
                                        GString *s = text_compact(e->content);
                                        if (!g_string_equal(s, t)) g_string_free(s, TRUE);
                                        else {
                                                id = i;
                                                g_string_free(s, TRUE);
                                                break;
                                        }
                                }
                        }
                        if (id >= 0) break; 
                }
                if (id < 0) {
                        OrezElement *e = cf_node->data;
                        g_error("Line %zu: "
                                "These is no the code fragment with the label <%s>",
                                e->line_num, target_label->str);
                } else {
                        if (!operator) {
                                OrezElement *e = cf_node->data;
                                g_error("Line %zu: This code fragment needs an operator", e->line_num);
                        } else if (g_str_equal(operator->str, utf8_plus)) {
                                g_ptr_array_insert(tie->prog_order, id + 1, cf_node);
                        } else {
                                g_ptr_array_insert(tie->prog_order, id, cf_node);
                        }
                }
                g_string_free(t, TRUE);
        }
        g_string_free(key, TRUE);
}
static void build_ref_relation(GHashTable *table, GNode *u, OrezElement *x)
{
        GString *a = text_compact(x->content);
        OrezTie *q = g_hash_table_lookup(table, a);
        if (q) g_ptr_array_add(q->emissions, u);
        else {
                g_error("Line %zu: Code fragment named <%s> never existed!",
                        x->line_num, x->content->str);
        }
        g_string_free(a, TRUE);
}
static gboolean start_of_line(gchar *pos, gchar *head)
{
        if (!pos) return FALSE;
        if (pos == head) return TRUE;
        pos = g_utf8_prev_char(pos);
        gunichar c = g_utf8_get_char(pos);
        return (c == ucs_linebreak) ? TRUE : FALSE;
}
static GString *cascade_indents(GList *indents)
{
        GString *indent = g_string_new(NULL);
        while (indents) {
                g_string_append(indent, ((GString *)(indents->data))->str);
                indents = indents->next;
        }
        return indent;
}
static GString *text_chug(GString *text)
{
        gchar *pos = TEXT_HEAD(text);
        gchar *tail = TEXT_TAIL(text);
        gunichar c;
        while (pos != tail) {
                c = g_utf8_get_char(pos);
                if (!char_belong_to(c, " \t\n")) break;
                pos = g_utf8_next_char(pos);
        }
        GString *new_text = g_string_new(NULL);
        while (pos != tail) {
                c = g_utf8_get_char(pos);
                g_string_append_unichar(new_text, c);
                pos = g_utf8_next_char(pos);
        }
        return new_text;
}
static GString *text_chomp(GString *text)
{
        gchar *head = TEXT_HEAD(text);
        gchar *pos = TEXT_TAIL(text);
        gunichar c;
        while (pos != head) {
                pos = g_utf8_prev_char(pos);
                c = g_utf8_get_char(pos);
                if (!char_belong_to(c, " \t\n")) {
                        pos = g_utf8_next_char(pos);
                        break;
                }
        }        
        GString *new_text = g_string_new(NULL);
        gchar *tail = pos;
        pos = head;
        while (pos != tail) {
                c = g_utf8_get_char(pos);
                g_string_append_unichar(new_text, c);
                pos = g_utf8_next_char(pos);
        }
        return new_text;
}

static GString *text_strip(GString *text)
{
        GString *a = text_chug(text);
        GString *b = text_chomp(a);
        g_string_free(a, TRUE);
        return b;
}
static void clean_doc_frag(GNode *x)
{
        OrezElement *e = x->data;
        GString *t = e->content;
        GString *a = text_strip(t);
        e->content = a;
        g_string_free(t, TRUE);
}
static void clean_code_frag(GNode *x)
{
        EVERY_BRANCH(x, it) {
                if (((OrezElement *)(it->data))->type == OREZ_CF_BODY) {
                        OrezElement *e = NULL;
                        EVERY_BRANCH(it, o_it) {
                                e = o_it->data;
                                if (e->type == OREZ_CF_REF) {
                                        GString *t = e->content;
                                        GString *a = text_strip(t);
                                        e->content = a;
                                        g_string_free(t, TRUE);
                                }
                        }
                        GString *t = e->content;
                        e->content = text_chomp(t);
                        g_string_free(t, TRUE);
                } else {
                        OrezElement *e = it->data;
                        GString *t = e->content;
                        e->content = text_strip(t);
                        g_string_free(t, TRUE);
                }
        }
}
static void clean_syntax_tree(GNode *root)
{
        EVERY_BRANCH(root, it) {
                OrezElement *e = it->data;
                if (e->type == OREZ_DOC_FRAG) clean_doc_frag(it);
                else clean_code_frag(it);
        }
}
static void yaml_append_key(GString *yaml, gchar *str, guint level)
{
        for (guint i = 0; i < level; i++) {
                g_string_append(yaml, yaml_indent);
        }
        g_string_append(yaml, str);
}

static void yaml_append_val(GString *yaml,
                            GString *value,
                            guint level,
                            gboolean multi_lines)
{
        gchar *head = TEXT_HEAD(value);
        gchar *tail = TEXT_TAIL(value);
        gchar *pos = head;
        gunichar c;
        for (guint i = 0; i < level; i++) {
                g_string_append(yaml, yaml_indent);
        }
        if (multi_lines) g_string_append(yaml, "'");
        while (pos != tail) {
                TEXT_READ(pos, c);
                g_string_append_unichar(yaml, c);
                if (c == ucs_linebreak) {
                        for (guint i = 0; i < level; i++) {
                                g_string_append(yaml, yaml_indent);
                        }
                }
        }
        if (multi_lines) g_string_append(yaml, "'");
        g_string_append(yaml, "\n");
}
static GString *doc_frag_to_yaml(GNode *x)
{
        OrezElement *e = x->data;
        GString *yaml = g_string_new("- DOC_FRAG: |-\n");
        yaml_append_val(yaml, ((OrezElement *)(x->data))->content, 1, TRUE);
        return yaml;
}
static GString *code_frag_to_yaml(GNode *x, GHashTable *table)
{
        OrezElement *x_cf_name = g_node_first_child(x)->data;
        GString *x_tie_key = text_compact(x_cf_name->content);
        OrezTie *x_tie = g_hash_table_lookup(table, x_tie_key);
        g_string_free(x_tie_key, TRUE);        
        GString *yaml = g_string_new("- CODE_FRAG:\n");
        EVERY_BRANCH(x, it) {
                OrezElement *e = it->data;
                switch(e->type) {
                case OREZ_CF_NAME:
                        yaml_append_key(yaml, "NAME: |-\n", 1);
                        yaml_append_val(yaml, e->content, 2, TRUE);
                        do { /* get ID and convert it to YAML object */        
                                if (x_tie->doc_order->len == 1) break;
                                for (guint i = 0; i < x_tie->doc_order->len; i++) {
                                        if (x == g_ptr_array_index(x_tie->doc_order, i)) {
                                                yaml_append_key(yaml, "ID: ", 1); 
                                                g_string_append_printf(yaml, "%u\n", i + 1);
                                                break;
                                        }
                                }
                        } while (0);
                        break;
                case OREZ_CF_LANG_MARK:
                        yaml_append_key(yaml, "LANG: ", 1);
                        yaml_append_val(yaml, e->content, 0, FALSE);
                        break;
                case OREZ_CF_LABEL_REF:
                        yaml_append_key(yaml, "LABEL_REF: |-\n", 1);
                        yaml_append_val(yaml, e->content, 2, TRUE);
                        break;
                case OREZ_CF_OPERATOR:
                        yaml_append_key(yaml, "OPERATOR: ", 1);
                        yaml_append_val(yaml, e->content, 0, FALSE);
                        break;
                case OREZ_CF_LABEL:
                        yaml_append_key(yaml, "LABEL: |-\n", 1);
                        yaml_append_val(yaml, e->content, 2, TRUE);
                        break;
                case OREZ_CF_BODY:
                        yaml_append_key(yaml, "CONTENT:\n", 1);
                        EVERY_BRANCH(it, o_it) {
                                OrezElement *o_e = o_it->data;
                                if (o_e->type == OREZ_CF_SNIPPET) {
                                        yaml_append_key(yaml, "- SNIPPET: |-\n", 2);
                                        yaml_append_val(yaml, o_e->content, 3, TRUE);
                                } else if (o_e->type == OREZ_CF_INDENT) {
                                        yaml_append_key(yaml, "- INDENT: ", 2);
                                        g_string_append_printf(yaml, "'%s'\n", o_e->content->str);
                                } else {
                                        yaml_append_key(yaml, "- REF:\n", 2);
                                        yaml_append_key(yaml, "NAME: |-\n", 3);
                                        yaml_append_val(yaml, o_e->content, 4, TRUE);
                                        GString *y_tie_key = text_compact(o_e->content);
                                        OrezTie *y_tie = g_hash_table_lookup(table, y_tie_key);
                                        if (y_tie->doc_order->len > 1) {
                                                yaml_append_key(yaml, "ID_ARRAY:\n", 3);
                                                for (guint i = 0; i < y_tie->doc_order->len; i++) {
                                                        GNode *t = g_ptr_array_index(y_tie->doc_order, i);
                                                        OrezElement *oo_e = t->data;
                                                        yaml_append_key(yaml, "- ", 4);
                                                        g_string_append_printf(yaml, "%u\n", i + 1);
                                                }
                                        }
                                        g_string_free(y_tie_key, TRUE);
                                }
                        }
                        break;
                default:
                        g_warning("code_frag_to_yaml: Unknown element type!");
                }
        }
        if (x_tie->emissions->len > 0) {
                yaml_append_key(yaml, "EMISSIONS:\n", 1);
                for (guint i = 0; i < x_tie->emissions->len; i++) {
                        GNode *emission = g_ptr_array_index(x_tie->emissions, i);
                        OrezElement *o_e = g_node_first_child(emission)->data;
                        GString *y_tie_key = text_compact(o_e->content);
                        OrezTie *y_tie = g_hash_table_lookup(table, y_tie_key);
                        yaml_append_key(yaml, "- EMISSION:\n", 2);
                        for (guint j = 0; j < y_tie->doc_order->len; j++) {
                                if (emission == g_ptr_array_index(y_tie->doc_order, j)) {
                                        yaml_append_key(yaml, "NAME: |-\n", 3);
                                        yaml_append_val(yaml, o_e->content, 4, TRUE);
                                        if (y_tie->doc_order->len > 1) {
                                                /* output ID for code fragment of same name */
                                                yaml_append_key(yaml, "ID: ", 3);
                                                g_string_append_printf(yaml, "%u\n", j + 1);
                                        }
                                        break;
                                }
                        }
                        g_string_free(y_tie_key, TRUE);
                }
        }
        return yaml;
}
static GList *obtain_thread(GHashTable *table, GString *start, GList *thread)
{
        GString *key = text_compact(start);
        OrezTie *tie = g_hash_table_lookup(table, key);
        for (guint i = 0; i < tie->doc_order->len; i++) {
                GNode *t = g_ptr_array_index(tie->doc_order, i);
                GNode *x = g_node_last_child(t);
                thread = g_list_prepend(thread, t);
                EVERY_BRANCH(x, it) {
                        OrezElement *e = it->data;
                        if (e->type == OREZ_CF_REF) {
                                thread = obtain_thread(table, e->content, thread);
                        }
                }
        }
        g_string_free(key, TRUE);
        return thread;
}
static void spread_lang_mark_in_thread(GList *thread)
{
        GString *lang_mark = NULL;
        for (GList *it = thread; it != NULL; it = it->next) {
                GNode *t = it->data;
                EVERY_BRANCH(t, o_it) {
                        OrezElement *e = o_it->data;
                        if (e->type == OREZ_CF_LANG_MARK) {
                                lang_mark = e->content;
                                break;
                        }
                }
        }
        if (lang_mark) {
                for (GList *it = thread; it != NULL; it = it->next) {
                        GNode *t = it->data;
                        GString *o_lang_mark = NULL;
                        EVERY_BRANCH(t, o_it) {
                                OrezElement *e = o_it->data;
                                if (e->type == OREZ_CF_LANG_MARK) {
                                        o_lang_mark = e->content;
                                        break;
                                }
                        }
                        if (o_lang_mark) {
                                if (!g_string_equal(lang_mark, o_lang_mark)) {
                                        GNode *o_t = g_node_first_child(t);
                                        OrezElement *name = o_t->data;
                                        g_error("There are two different language marks"
                                                "occuring in a thread: <%s> and <%s>",
                                                lang_mark->str, o_lang_mark->str);
                                }
                        } else {
                                OrezElement *content = g_node_last_child(g_node_last_child(t))->data;
                                OrezElement *new_e = g_slice_new(OrezElement);
                                new_e->type = OREZ_CF_LANG_MARK;
                                new_e->line_num = content->line_num - 1;
                                new_e->content = g_string_new(lang_mark->str);
                                g_node_insert_data_after(t, g_node_first_child(t), new_e);
                        }
                }
        }
}
static void spread_lang_mark(GNode *syntax_tree, GHashTable *table)
{
        EVERY_BRANCH(syntax_tree, it) {
                OrezElement *e = it->data;
                if (e->type == OREZ_CODE_FRAG) {
                        OrezElement *o_e = g_node_first_child(it)->data;
                        GList *thread = obtain_thread(table,
                                                      o_e->content,
                                                      NULL);
                        spread_lang_mark_in_thread(thread);
                        g_list_free(thread);
                }
        }
        /* check */
        EVERY_BRANCH(syntax_tree, it) {
                OrezElement *e = it->data;
                if (e->type == OREZ_CODE_FRAG) {
                        OrezElement *name = g_node_first_child(it)->data;
                        gboolean has_lang_mark = FALSE;
                        EVERY_BRANCH(it, o_it) {
                                OrezElement *o_e = o_it->data;
                                if (o_e->type == OREZ_CF_LANG_MARK) {
                                        has_lang_mark = TRUE;
                                        break;
                                }
                        }
                        if (!has_lang_mark) {
                                g_warning("Line %zu: Programing Language of"
                                          "<@ %s #> is unknown!",
                                          e->line_num, name->content->str);
                        }
                }
        }
}
GNode *orez_create_syntax_tree(gchar *file_name)
{
        return stage_3rd(stage_2nd(stage_1st(file_name)));
}
void orez_destroy_syntax_tree(GNode *syntax_tree)
{
        destroy_syntax_elements(syntax_tree);
        g_node_destroy(syntax_tree);
}
GHashTable *orez_create_hash_table(GNode *syntax_tree)
{
        GHashTable *table = g_hash_table_new((GHashFunc)g_string_hash,
                                             (GEqualFunc)g_string_equal);
        EVERY_BRANCH(syntax_tree, it) {
                OrezElement *e = it->data;
                if (e->type != OREZ_CODE_FRAG) continue;
                hash_table_wrap(table, it);
        }
        EVERY_BRANCH(syntax_tree, it) {
                OrezElement *e = it->data;
                if (e->type != OREZ_CODE_FRAG) continue;
                GNode *body_node = g_node_last_child(it);
                EVERY_BRANCH(body_node, o_it) {
                        OrezElement *o_e = o_it->data;
                        if (o_e->type != OREZ_CF_REF) continue;
                        build_ref_relation(table, it, o_e);
                }
        }
        return table;
}
void orez_destroy_hash_table(GHashTable *table)
{
        GList *keys = g_hash_table_get_keys(table);
        GList *keys_it = keys;
        while (keys_it) {
                GString *key = keys_it->data;
                OrezTie *tie = g_hash_table_lookup(table, key);
                g_ptr_array_free(tie->doc_order, TRUE);
                g_ptr_array_free(tie->prog_order, TRUE);
                g_ptr_array_free(tie->emissions, TRUE);
                g_slice_free(OrezTie, tie);
                g_string_free(key, TRUE);
                keys_it = keys_it->next;
        }
        g_list_free(keys);
        g_hash_table_destroy(table);
}
void orez_tangle(GNode *syntax_tree,
                 GHashTable *relations,
                 GString *starting_point_name,
                 GList **indents,
                 GIOChannel *output)
{
        GString *key = text_compact(starting_point_name);
        OrezTie *tie = g_hash_table_lookup(relations, key);
        if (!tie) {
                g_error("Code fragment <%s> never existed!",
                        starting_point_name->str);
        }
        g_string_free(key, TRUE);
        
        for (guint i = 0; i < tie->prog_order->len; i++) {
                GNode *cf = g_ptr_array_index(tie->prog_order, i);
                GNode *body = g_node_last_child(cf);
                EVERY_BRANCH(body, it) {
                        OrezElement *e = it->data;
                        if (e->type == OREZ_CF_SNIPPET) {
                                GString *indent = cascade_indents(*indents);
                                if (orez_show_line_num) {
                                        GString *ln = g_string_new(NULL);
                                        g_string_printf(ln,
                                                        "#line %zu \"%s\"\n",
                                                        e->line_num,
                                                        (gchar *)(syntax_tree->data));
                                        FILE_WRITE_TEXT(output, ln);
                                        g_string_free(ln, TRUE);
                                }
                                gchar *head = TEXT_HEAD(e->content);
                                gchar *tail = TEXT_TAIL(e->content);
                                gchar *pos = head;
                                gunichar c;
                                while (pos != tail) {
                                        if (start_of_line(pos, head)) FILE_WRITE_TEXT(output, indent);
                                        TEXT_READ(pos, c);
                                        FILE_WRITE(output, c);
                                }
                                g_string_free(indent, TRUE);
                        } else if (e->type == OREZ_CF_REF) {
                                GNode *prev = g_node_prev_sibling(it);
                                if (prev) {
                                        OrezElement *o_e = prev->data;
                                        if (o_e->type == OREZ_CF_INDENT) {
                                                *indents = g_list_prepend(*indents, o_e->content);
                                        }
                                }
                                orez_tangle(syntax_tree, relations, e->content, indents, output);
                                *indents = g_list_delete_link(*indents, *indents);
                        }
                }
        }   
}
int main(int argc, char **argv)
{
        setlocale(LC_ALL, "");
        GOptionContext *context = g_option_context_new("orez-file");
        g_option_context_add_main_entries(context, orez_entries, NULL);
        if (!g_option_context_parse(context, &argc, &argv, NULL)) return -1;
        if (argv[1] == NULL) g_error("You should provide an orez file!\n");
        if (orez_tangle_mode && orez_weave_mode) {
                g_error("Both tangle and weave can not be turned on!");
        }
        if (orez_tangle_mode) {
                if (!orez_entrance) {
                        g_error("You should provided "
                                "the start of thread to be tangled");
                }
        }
                GNode *syntax_tree = orez_create_syntax_tree(argv[1]);
        GHashTable *tie_table = orez_create_hash_table(syntax_tree);
        if (orez_tangle_mode) {
                GPtrArray *entrances = g_ptr_array_new();
                GPtrArray *file_names = g_ptr_array_new();
                gchar *u = orez_output ? orez_output : orez_entrance;
                if (!orez_separator) {
                        GString *t = g_string_new(u);
                        g_ptr_array_add(entrances, g_string_new(orez_entrance));
                        g_ptr_array_add(file_names, text_compact(t));
                        g_string_free(t, TRUE);
                } else {
                        gchar **cf_names = g_strsplit(orez_entrance, orez_separator, 0);
                        gchar **v = g_strsplit(u, orez_separator, 0);
                        guint m = 0; for (gchar **s = cf_names; *s != NULL; s++) m++;
                        guint n = 0; for (gchar **s = v; *s != NULL; s++) n++;
                        for (guint i = 0; i < m; i++) {
                                GString *t = g_string_new(v[i]);
                                g_ptr_array_add(entrances, g_string_new(cf_names[i]));
                                g_ptr_array_add(file_names, text_compact(t));
                                g_string_free(t, TRUE);
                        }
                        g_strfreev(v);
                        g_strfreev(cf_names);
                }
                for (guint i = 0; i < file_names->len; i++) {
                        GString *start = g_ptr_array_index(entrances, i);
                        GString *file_name = g_ptr_array_index(file_names, i);
                        GIOChannel *output = g_io_channel_new_file(file_name->str, "w", NULL);
                        GList *indents = NULL;
                        orez_tangle(syntax_tree, tie_table, start, &indents, output);
                        g_io_channel_unref(output);
                        g_string_free(file_name, TRUE);
                        g_string_free(start, TRUE);
                }
                g_ptr_array_free(file_names, TRUE);
                g_ptr_array_free(entrances, TRUE);
        } else {
                clean_syntax_tree(syntax_tree);
                spread_lang_mark(syntax_tree, tie_table);
                GIOChannel *output = NULL;
                if (orez_output) output = g_io_channel_new_file(orez_output, "w", NULL);
                EVERY_BRANCH(syntax_tree, it) {
                        OrezElement *e = it->data;
                        GString *s = NULL;
                        if (e->type == OREZ_DOC_FRAG) s = doc_frag_to_yaml(it);
                        else s = code_frag_to_yaml(it, tie_table);
                        if (output) g_io_channel_write_chars(output, s->str, -1, NULL, NULL);
                        else g_print("%s", s->str);
                        g_string_free(s, TRUE);
                }
                if (orez_output) g_io_channel_unref(output);
        }
        orez_destroy_hash_table(tie_table);
        orez_destroy_syntax_tree(syntax_tree);
        g_option_context_free(context);
        return 0;
}
