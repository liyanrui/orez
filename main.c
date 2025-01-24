#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <locale.h>
#include <cyaml/cyaml.h>
#include <glib.h>
typedef enum  {
        OREZ_SNIPPET_DELIMITER, /* 文档或源码片段界限符 */
        OREZ_SNIPPET_NAME_DELIMITER, /* 片段名字界限符 */
        OREZ_LANGUAGE_BEGINNING_MARK, /* 语言标记开始符 */
        OREZ_LANGUAGE_END_MARK, /* 语言标记终结符 */
        OREZ_SNIPPET_APPENDING_MARK, /* 后向合并运算符 */
        OREZ_SNIPPET_PREPENDING_MARK, /*前向合并运算符 */
        OREZ_TAG_BEGINNING_MARK, /* 代码片段标签开始符 */
        OREZ_TAG_END_MARK, /* 代码片段标签终结符 */
        OREZ_SNIPPET_REFERENCE_BEGINNING_MARK, /* 代码片段引用开始符 */
        OREZ_SNIPPET_REFERENCE_END_MARK, /* 代码片段引用终结符 */
        OREZ_TEXT /* 普通文本 */
} OrezTokenType;
typedef struct {
        OrezTokenType type; /* 记号类型标识 */
        gsize line_number;  /* 记号对应文本在全文中的行号 */
        GString *content;   /* 记号对应的文本内容 */
} OrezToken;
typedef struct {
        char *snippet_delimiter;
        char *snippet_name_delimiter;
        char *snippet_name_continuation;
        char *language_beginning_mark;
        char *language_end_mark;
        char *snippet_appending_mark;
        char *snippet_prepending_mark;
        char *tag_beginning_mark;
        char *tag_end_mark;
        char *snippet_reference_beginning_mark;
        char *snippet_reference_end_mark;
} OrezConfig;
typedef struct {
        GString *snippet_delimiter;
        GString *snippet_name_delimiter;
        GString *snippet_name_continuation;
        GString *language_beginning_mark;
        GString *language_end_mark;
        GString *snippet_appending_mark;
        GString *snippet_prepending_mark;
        GString *tag_beginning_mark;
        GString *tag_end_mark;
        GString *snippet_reference_beginning_mark;
        GString *snippet_reference_end_mark;
} OrezSymbols;
static int read_char_from_input(FILE *input, GString *cache)
{
        int c = fgetc(input);
        if (c != EOF) {
                g_string_append_c(cache, (gchar)c);
                return c;
        } else return EOF;
}
static bool am_i_here(char *a, char *b, char **cursor, char *me, int dir)
{
        size_t n = strlen(me);
        char *p = *cursor;
        /* 若 cursor 超范围，无需匹配 */
        if (dir < 0 && p < a + n - 1) return false;
        if (dir >= 0 && p > b - n + 1) return false;
        /* 否则 */
        bool result = false;
        char *t = malloc((n + 1) * sizeof(char));
        for (size_t i = 0; i < n; i++) {
                t[i] = (dir < 0) ? *(p + i - n + 1) : *(p + i);
        }
        t[n] = '\0';
        if (strcmp(t, me) == 0) {
                result = true;
                *cursor = (dir < 0) ? (p - n + 1) : (p + n - 1);
        }
        free(t);
        return result;
}
static GString *make_padding(size_t len)
{
        GString *padding = g_string_new(NULL);
        for (size_t i = 0; i < len; i++) {
                g_string_append_c(padding, ' ');
        }
        return padding;
}
static bool tail_is_snippet_delimiter(GString *cache,
                                      GString *snippet_delimiter)
{
        char *p = NULL;
        /* 若 cache 末尾不存在片段界限符 */
        if (snippet_delimiter->len > cache->len) return false;
        p = cache->str + cache->len - snippet_delimiter->len;
        if (strcmp(p, snippet_delimiter->str) != 0) return false;
        /* 否则 */
        enum {IDLE, SPACE, SUCCESS, FAILURE} state = IDLE;
        if (p == cache->str) return true;
        else p--;
        /* 构造字符串区间 [a, b] */
        char *a = cache->str, *b = p;
        /* 从 b 开始逆序遍历 [a, b] */
        while (1) {
                switch (state) {
                case IDLE:
                        if (*p == ' ') state = SPACE;
                        else if (*p == '\t') state = SPACE;
                        else if (am_i_here(a, b, &p, "　", -1)) {
                                state = SPACE;
                        }
                        else if (*p == '\n') state = SUCCESS;
                        else state = FAILURE;
                        break;
                case SPACE:
                        if (*p == ' ') state = SPACE;
                        else if (*p == '\t') state = SPACE;
                        else if (am_i_here(a, b, &p, "　", -1)) state = SPACE;
                        else if (*p == '\n') state = SUCCESS;
                        else state = FAILURE;
                        break;
                default:
                        g_error("Illegal state in <<< %s >>>", cache->str);
                }
                if (p == a) {
                        /* 字符串逆序遍历到头，此时匹配过程既未成功，
                           亦未失败，则特命其成功 */
                        state = SUCCESS;
                }
                if (state == FAILURE || state == SUCCESS) break;
                else p--;
        }
        return (state == SUCCESS) ? true : false;
}
static GList *orez_snippet(GList *tokens, GString *cache)
{
        OrezToken *snippet = malloc(sizeof(OrezToken));
        snippet->type = OREZ_TEXT;
        size_t n;
        GList *last = g_list_last(tokens);
        if (last) {
                OrezToken *last_token = last->data;
                n = last_token->line_number;
                for (char *p = last_token->content->str; *p != '\0'; p++) {
                        if (*p == '\n') {
                                n++;
                        }
                }
        } else {
                n = 1;
        }
        snippet->line_number = n;
        snippet->content = cache;
        tokens = g_list_append(tokens, snippet);
        return tokens;
}
static GList *orez_snippet_delimiter(GList *tokens, GString *snippet_delimiter)
{
        OrezToken *snippet = malloc(sizeof(OrezToken));
        snippet->type = OREZ_SNIPPET_DELIMITER;
        size_t n;
        GList *last = g_list_last(tokens);
        if (last) {
                OrezToken *last_token = last->data;
                n = last_token->line_number;
                for (char *p = last_token->content->str; *p != '\0'; p++) {
                        if (*p == '\n') {
                                n++;
                        }
                }
        } else {
                n = 1;
        }
        snippet->line_number = n;
        snippet->content = g_string_new(snippet_delimiter->str);
        tokens = g_list_append(tokens, snippet);
        return tokens;
}
static void print_tokens(GList *tokens)
{
        for (GList *p = g_list_first(tokens);
             p != NULL;
             p = p->next) {
                OrezToken *t = p->data;
                switch (t->type) {
                case OREZ_SNIPPET_DELIMITER:
                        printf("snippet delimiter");
                        break;
                case OREZ_SNIPPET_NAME_DELIMITER:
                        printf("snippet name delimiter");
                        break;
                case OREZ_LANGUAGE_BEGINNING_MARK:
                        printf("language beginning mark");
                        break;
                case OREZ_LANGUAGE_END_MARK:
                        printf("language end mark");
                        break;
                case OREZ_SNIPPET_APPENDING_MARK:
                        printf("snippet appending mark");
                        break;
                case OREZ_SNIPPET_PREPENDING_MARK:
                        printf("snippet prepending mark");
                        break;
                case OREZ_TAG_BEGINNING_MARK:
                        printf("tag beginning mark");
                        break;
                case OREZ_TAG_END_MARK:
                        printf("tag end mark");
                        break;
                case OREZ_SNIPPET_REFERENCE_BEGINNING_MARK:
                        printf("snippet reference beginning mark");
                        break;
                case OREZ_SNIPPET_REFERENCE_END_MARK:
                        printf("snippet reference end mark");
                        break;
                case OREZ_TEXT:
                        printf("text");
                        break;
                default:
                        printf("Illegal token!\n");
                }
                printf(" | line %lu: %s\n", t->line_number, t->content->str);
        }
}
static void delete_tokens(GList *tokens)
{
        for (GList *p = g_list_first(tokens);
             p != NULL;
             p = p->next) {
                OrezToken *t = p->data;
                g_string_free(t->content, TRUE);
                free(t);
        }
        g_list_free(tokens);
}
static char *find_name_delimiter(OrezToken *t,
                                 GString *delimiter,
                                 GString *continuation)
{
        /* 构造字符串区间 [a, b] */
        char *a = t->content->str;
        if (*a == '\n') return NULL;
        /* b 为 NULL 或指向片段名字界限符首字节 */
        char *b = strstr(a, delimiter->str);
        if (!b || b == a) return NULL;
        /* 检测 source 是否含有合法的片段名字界限符 */
        char *p = --b;
        enum {IDLE, LINEBREAK, SUCCESS, FAILURE} state = IDLE;
        while (1) { /* 逆序遍历 [a, b] */
                switch (state) {
                case IDLE:
                        if (*p == '\n') state = LINEBREAK;
                        break;
                case LINEBREAK:
                        if (*p == ' '
                            || *p == '\t'
                            || am_i_here(a, b, &p, "　", -1)) state = LINEBREAK;
                        else if (am_i_here(a, b, &p, continuation->str, -1)) {
                                state = IDLE;
                        } else state = FAILURE;
                        break;
                default:
                        g_error("Illegal state in line %lu.", t->line_number);
                }
                if (p == a) {
                        /* 字符串逆序遍历到头，此时匹配过程既未成功，
                           亦未失败，则特命其成功 */
                        state = SUCCESS;
                }
                if (state == FAILURE || state == SUCCESS) break;
                else p--;
        }
        return (state == SUCCESS) ? b + 1 : NULL;
}
static GString *extract_small_block_at_head(OrezToken *t,
                                            GString *beginning_mark,
                                            GString *end_mark)
{
        GString *result = NULL;
        enum {IDLE, MAYBE_MARK, FAILURE, SUCCESS} state = IDLE;
        char *block_beginning = NULL;
        char *block_end = NULL;
        size_t new_line_number = t->line_number;
        /* 构造字符串区间 [a, b] */
        char *a = t->content->str, *b = a + t->content->len - 1;
        if (a == b) return NULL;
        char *p = a;
        while (1) { /* 正序遍历 [a, b] */
                switch (state) {
                case IDLE:
                        if (*p == ' ' || *p == '\t'
                            || am_i_here(a, b, &p, "　", 1)) {
                                state = IDLE;
                        } else if (*p == '\n') {
                                new_line_number++;
                                state = IDLE;
                        } else if (am_i_here(a, b, &p, beginning_mark->str, 1)) {
                                block_beginning = p - beginning_mark->len + 1;
                                state = MAYBE_MARK;
                        } else state = FAILURE;
                        break;
                case MAYBE_MARK:
                        if (am_i_here(a, b, &p, beginning_mark->str, 1)) {
                                state = FAILURE;
                        } else if (am_i_here(a, b, &p, end_mark->str, 1)) {
                                block_end = p + 1;
                                state = SUCCESS;
                        } else state = MAYBE_MARK;
                        break;
                default:
                        g_error("Illegal state in line %lu.",
                                t->line_number);
                }
                if (state == SUCCESS) {
                        result = g_string_new(NULL);
                        for (char *q = block_beginning; q != block_end; q++) {
                                g_string_append_c(result, *q);
                        }
                        /* 从 t 的内容中删除语言标记 */
                        GString *new_content = g_string_new(NULL);
                        for (char *q = block_end; *q != '\0'; q++) {
                                g_string_append_c(new_content, *q);
                        }
                        g_string_free(t->content, TRUE);
                        t->content = new_content;
                        t->line_number = new_line_number;
                        break;
                } else if (state == FAILURE || *p == '\0') break;
                else p++;
        }
        return result;
}
static GString *extract_block_content(GString *block,
                                      GString *beginning_mark,
                                      GString *end_mark)
{
        GString *content = g_string_new(NULL);
        size_t a = beginning_mark->len;
        size_t b = block->len - end_mark->len;
        for (size_t i = a; i < b; i++) {
                g_string_append_c(content, *(block->str + i));
        }
        return content;
}
static GList *create_block_token(GList *tokens,
                                 GList *x,
                                 GString *block,
                                 GString *beginning_mark,
                                 OrezTokenType beginning_mark_type,
                                 GString *end_mark,
                                 OrezTokenType end_mark_type)
{
        OrezToken *t = x->data;
        /* 构建块起始记号 */
        OrezToken *beginning = malloc(sizeof(OrezToken));
        beginning->type = beginning_mark_type;
        beginning->line_number = t->line_number;
        beginning->content = g_string_new(beginning_mark->str);
        tokens = g_list_insert_before(tokens, x, beginning);
        /* 构建块内容记号 */
        OrezToken *body = malloc(sizeof(OrezToken));
        body->type = OREZ_TEXT;
        body->line_number = t->line_number;
        body->content = extract_block_content(block, beginning_mark, end_mark);
        tokens = g_list_insert_before(tokens, x, body);
        /* 构建块终止记号 */
        OrezToken *end = malloc(sizeof(OrezToken));
        end->type = end_mark_type;
        end->line_number = t->line_number;
        end->content = g_string_new(end_mark->str);
        tokens = g_list_insert_before(tokens, x, end);
        return tokens;
}
static GString *extract_operator(OrezToken *t, GString *operator)
{
        GString *result = NULL;
        /* 构造字符串区间 [a, b) */
        char *a = t->content->str;
        char *b = strstr(a, operator->str);
        if (!b) return result;
        bool is_legal = true;
        if (b == t->content->str) ;
        else {
                enum {IDLE, FAILURE} state = IDLE;
                char *p = b - 1;
                /* 以 p 逆序遍历 [a, b) */
                while (1) {
                        switch (state) {
                        case IDLE:
                                if (*p == ' '
                                    || *p == '\t'
                                    || *p == '\n'
                                    || am_i_here(a, b - 1, &p, "　", -1)) ;
                                else state = FAILURE;
                                break;
                        default:
                                g_error("Illegal state in line %lu.",
                                        t->line_number);
                        }
                        if (p == a) break;
                        else if (state == FAILURE) {
                                is_legal = false;
                                break;
                        } else p--;
                }
        }
        if (is_legal) {
                /* 从 t 中删除片段追加运算符及其之前的空白字符 */
                GString *new_content = g_string_new(NULL);
                size_t new_line_number = t->line_number;
                for (char *p = a; p != b; p++) {
                        if (*p == '\n') new_line_number++;
                }
                for (char *p = b + operator->len; *p != '\0'; p++) {
                        g_string_append_c(new_content, *p);
                }
                g_string_free(t->content, TRUE);
                t->content = new_content;
                t->line_number = new_line_number;
                /* 构造片段追加运算符副本 */
                result = g_string_new(operator->str);
        }
        return result;
}
static GList *create_operator_token(GList *tokens,
                                    GList *x,
                                    GString *operator,
                                    OrezTokenType operator_type)
{
        OrezToken *t = x->data;
        OrezToken *a = malloc(sizeof(OrezToken));
        a->type = operator_type;
        a->line_number = t->line_number;
        a->content = operator;
        tokens = g_list_insert_before(tokens, x, a);
        return tokens;
}
typedef struct {
        void *a;
        void *b;
} OrezPair;
static OrezPair *find_snippet_reference(GString *content,
                                        GString *reference_beginning_mark,
                                        GString *reference_end_mark,
                                        GString *name_continuation)
{
        char *a = content->str, *begin = NULL, *end = NULL;
        enum {IDLE, MAYBE_LINEBREAK, FAILURE} state;
        while (1) {
                begin = strstr(a, reference_beginning_mark->str);
                if (!begin) return NULL;
                end = strstr(a, reference_end_mark->str);
                if (!end || begin >= end) return NULL;
                /*区间 [q, p] 是片段名 */
                char *p = end - 1;
                char *q = begin + reference_beginning_mark->len;
                /* 逆序遍历 [q, end)，校验片段名是否合法 */
                bool legal = TRUE;
                state = IDLE;
                while (1) {
                        switch (state) {
                        case IDLE:
                                if (*p == '\n') state = MAYBE_LINEBREAK;
                                break;
                        case MAYBE_LINEBREAK:
                                if (*p == ' '
                                    || am_i_here(q, end - 1, &p, "　", -1)) ;
                                else if (am_i_here(q, end - 1, &p,
                                                   name_continuation->str, -1)) {
                                        state = IDLE;
                                } else state = FAILURE;
                                break;
                        default:
                                g_error("Illegal state in <<< %s >>>.", a);
                        }
                        if (state == FAILURE) {
                                legal = FALSE;
                                break;
                        } else if (p == q) break;
                        else p--;
                }
                if (legal) break;
                else a = q;
        }
        if (begin && end) {
                /* 前闭后开区间 */
                OrezPair *result = malloc(sizeof(OrezPair));
                result->a = begin;
                result->b = end + reference_end_mark->len;
                return result;
        } else return NULL;
}
static GList *split_snippet(GList *tokens,
                            GList *x,
                            OrezPair *snippet_reference,
                            GString *snippet_name_continuation,
                            GString *snippet_reference_beginning_mark,
                            GString *snippet_reference_end_mark)
{
        OrezToken *t = x->data;
        size_t line_number = t->line_number;
        /* 片段引用之前的内容 */
        GString *a = g_string_new(NULL);
        for (char *p = t->content->str; p != snippet_reference->a; p++) {
                g_string_append_c(a, *p);
                if (*p == '\n') line_number++;
        }
        OrezToken *t_a = malloc(sizeof(OrezToken));
        t_a->type = OREZ_TEXT;
        t_a->line_number = t->line_number;
        t_a->content = a;
        /* 片段引用起始标记 */
        GString *b = g_string_new(snippet_reference_beginning_mark->str);
        OrezToken *t_b = malloc(sizeof(OrezToken));
        t_b->type = OREZ_SNIPPET_REFERENCE_BEGINNING_MARK;
        t_b->line_number = line_number;
        t_b->content = b;
        /* 片段名称 */
        GString *c = g_string_new(NULL);
        char *left = (char *)(snippet_reference->a)
                + snippet_reference_beginning_mark->len;
        char *right = (char *)(snippet_reference->b)
                - snippet_reference_end_mark->len;
        for (char *p = left; p != right; p++) {
                g_string_append_c(c, *p);
                if (*p == '\n') line_number++;
        }
        g_string_replace(c, snippet_name_continuation->str, "", 0);
        OrezToken *t_c = malloc(sizeof(OrezToken));
        t_c->type = OREZ_TEXT;
        t_c->line_number = t_b->line_number;
        t_c->content = c;
        /* 片段引用结束标记 */
        GString *d = g_string_new(snippet_reference_end_mark->str);
        OrezToken *t_d = malloc(sizeof(OrezToken));
        t_d->type = OREZ_SNIPPET_REFERENCE_END_MARK;
        t_d->line_number = line_number;
        t_d->content = d;
        /* 片段引用之后的内容 */
        GString *e = g_string_new(NULL);
        bool after_reference = TRUE;
        for (char *p = snippet_reference->b; *p != '\0'; p++) {
                if (after_reference) {
                         /* 忽略与片段引用之后与片段引用终止符同一行的文本 */
                        if (*p == '\n') {
                                after_reference = FALSE;
                                g_string_append_c(e, *p);
                        }
                } else g_string_append_c(e, *p);
        }
        /* 更新记号列表 */
        tokens = g_list_insert_before(tokens, x, t_a);
        tokens = g_list_insert_before(tokens, x, t_b);
        tokens = g_list_insert_before(tokens, x, t_c);
        tokens = g_list_insert_before(tokens, x, t_d);
        g_string_free(t->content, TRUE);
        t->content = e;
        t->line_number = line_number;
        return tokens;
}
static GList *orez_lexer(const char *input_file_name, OrezSymbols *symbols)
{
        FILE *input = fopen(input_file_name, "r");
        if (!input) {
                g_error("Failed to open input file!\n");
        }
        GList *tokens = NULL;
        GString *cache = g_string_new(NULL);
        while (1) {
                int status = read_char_from_input(input, cache);
                if (status == EOF) break;
                else {
                        bool t = tail_is_snippet_delimiter(cache, symbols->snippet_delimiter);
                        if (t) {
                                g_string_erase(cache, cache->len - symbols->snippet_delimiter->len, -1);
                                tokens = orez_snippet(tokens, cache);
                                tokens = orez_snippet_delimiter(tokens,
                                                                symbols->snippet_delimiter);
                                cache = g_string_new(NULL); /* 刷新 cache */
                        }
                }
        }
        if (cache->len > 0) {
                tokens = orez_snippet(tokens, cache);
        } else g_string_free(cache, TRUE);
        fclose(input);
        GList *it = g_list_first(tokens); /* 该指针在之后遍历 tokens 的过程中依然会被使用 */
        while (1) {
                if (!it) break;
                OrezToken *t = it->data;
                if (t->type == OREZ_TEXT) {
                        char *p = find_name_delimiter(t,
                                                      symbols->snippet_name_delimiter,
                                                      symbols->snippet_name_continuation);
                        if (p) {
                                /* 提取片段名字并消除（可能存在的）续行符 */
                                cache = g_string_new(NULL); /* 在第一阶段扫描过程中便已出现 */
                                for (char *q = t->content->str; q != p; q++) {
                                        g_string_append_c(cache, *q);
                                }
                                g_string_replace(cache, symbols->snippet_name_continuation->str, "", 0);
                                /* 构建片段名字记号 */
                                OrezToken *snippet_name = malloc(sizeof(OrezToken));
                                snippet_name->type = OREZ_TEXT;
                                snippet_name->line_number = t->line_number;
                                snippet_name->content = cache;
                                /* 构建片段名字界限符记号 */
                                OrezToken *snippet_name_delimiter = malloc(sizeof(OrezToken));
                                snippet_name_delimiter->type = OREZ_SNIPPET_NAME_DELIMITER;
                                snippet_name_delimiter->line_number = snippet_name->line_number;
                                for (char *q = snippet_name->content->str; *q != '\0'; q++) {
                                        if (*q == '\n') snippet_name_delimiter->line_number++;
                                }
                                snippet_name_delimiter->content =
                                        g_string_new(symbols->snippet_name_delimiter->str);
                                /* 构建片段内容记号 */
                                OrezToken *snippet = malloc(sizeof(OrezToken));
                                snippet->type = OREZ_TEXT;
                                snippet->line_number = snippet_name_delimiter->line_number;
                                cache = g_string_new(NULL);
                                for (char *q = p + symbols->snippet_name_delimiter->len;
                                     *q != '\0';
                                     q++) {
                                        g_string_append_c(cache, *q);
                                }
                                snippet->content = cache;
                                /* 删除 it */
                                GList *next = g_list_next(it);
                                tokens = g_list_remove_link(tokens, it);
                                g_string_free(t->content, TRUE);
                                free(t);
                                g_list_free(it);
                                /* 在 prev 之后插入上述新结点 */
                                /* next 为 NULL 时，会向链表尾部插入结点 */
                                tokens = g_list_insert_before(tokens, next, snippet_name);
                                tokens = g_list_insert_before(tokens, next, snippet_name_delimiter);
                                tokens = g_list_insert_before(tokens, next, snippet);
                                /* 调整迭代指针 */
                                it = next;
                        }
                }
                it = g_list_next(it);
        }
        it = g_list_first(tokens); /* 该指针在之后遍历 tokens 的过程中依然会被使用 */
        while (1) {
                if (!it) break;
                OrezToken *t = it->data;
                if (t->type == OREZ_TEXT) {
                        GList *prev = g_list_previous(it);
                        if (prev) {
                                OrezToken *a = prev->data;
                                if (a->type == OREZ_SNIPPET_NAME_DELIMITER) {
                                        do {
                                                GString *language = extract_small_block_at_head(t,
                                                                                                symbols->language_beginning_mark,
                                                                                                symbols->language_end_mark);
                                                if (language) {
                                                        language = g_string_ascii_down(language);
                                                        tokens = create_block_token(tokens,
                                                                                    it,
                                                                                    language,
                                                                                    symbols->language_beginning_mark,
                                                                                    OREZ_LANGUAGE_BEGINNING_MARK,
                                                                                    symbols->language_end_mark,
                                                                                    OREZ_LANGUAGE_END_MARK);
                                                        g_string_free(language, TRUE);
                                                }
                                                GString *tag_reference = extract_small_block_at_head(t,
                                                                                                     symbols->tag_beginning_mark,
                                                                                                     symbols->tag_end_mark);
                                                if (tag_reference) {
                                                        tokens = create_block_token(tokens,
                                                                                    it,
                                                                                    tag_reference,
                                                                                    symbols->tag_beginning_mark,
                                                                                    OREZ_TAG_BEGINNING_MARK,
                                                                                    symbols->tag_end_mark,
                                                                                    OREZ_TAG_END_MARK);
                                                        g_string_free(tag_reference, TRUE);
                                                }
                                        } while (0);
                                        do {
                                                GString *language = extract_small_block_at_head(t,
                                                                                                symbols->language_beginning_mark,
                                                                                                symbols->language_end_mark);
                                                if (language) {
                                                        language = g_string_ascii_down(language);
                                                        tokens = create_block_token(tokens,
                                                                                    it,
                                                                                    language,
                                                                                    symbols->language_beginning_mark,
                                                                                    OREZ_LANGUAGE_BEGINNING_MARK,
                                                                                    symbols->language_end_mark,
                                                                                    OREZ_LANGUAGE_END_MARK);
                                                        g_string_free(language, TRUE);
                                                }
                                                GString *tag_reference = extract_small_block_at_head(t,
                                                                                                     symbols->tag_beginning_mark,
                                                                                                     symbols->tag_end_mark);
                                                if (tag_reference) {
                                                        tokens = create_block_token(tokens,
                                                                                    it,
                                                                                    tag_reference,
                                                                                    symbols->tag_beginning_mark,
                                                                                    OREZ_TAG_BEGINNING_MARK,
                                                                                    symbols->tag_end_mark,
                                                                                    OREZ_TAG_END_MARK);
                                                        g_string_free(tag_reference, TRUE);
                                                }
                                        } while (0);
                                }
                        }
                }
                it = g_list_next(it);
        }
        it = g_list_first(tokens);
        while (1) {
                if (!it) break;
                OrezToken *t = it->data;
                if (t->type == OREZ_TEXT) {
                        GList *prev = g_list_previous(it);
                        if (prev) {
                                OrezToken *a = prev->data;
                                if (a->type == OREZ_SNIPPET_NAME_DELIMITER
                                    || a->type == OREZ_LANGUAGE_END_MARK
                                    || a->type == OREZ_TAG_END_MARK) {
                                        GString *appending_mark = extract_operator(t, symbols->snippet_appending_mark);
                                        if (appending_mark) {
                                                tokens = create_operator_token(tokens,
                                                                               it,
                                                                               appending_mark,
                                                                               OREZ_SNIPPET_APPENDING_MARK);
                                        } else {
                                                GString *prepending_mark
                                                        = extract_operator(t, symbols->snippet_prepending_mark);
                                                if (prepending_mark) {
                                                        tokens = create_operator_token(tokens,
                                                                                       it,
                                                                                       prepending_mark,
                                                                                       OREZ_SNIPPET_PREPENDING_MARK);
                                                }
                                        }
                                        GString *tag_mark
                                                = extract_small_block_at_head(t,
                                                                              symbols->tag_beginning_mark,
                                                                              symbols->tag_end_mark);
                                        if (tag_mark) {
                                                tokens = create_block_token(tokens,
                                                                            it,
                                                                            tag_mark,
                                                                            symbols->tag_beginning_mark,
                                                                            OREZ_TAG_BEGINNING_MARK,
                                                                            symbols->tag_end_mark,
                                                                            OREZ_TAG_END_MARK);
                                                g_string_free(tag_mark, TRUE);
                                        }
                                }
                        }
                }
                it = g_list_next(it);
        }
        it = g_list_first(tokens);
        while (1) {
                if (!it) break;
                OrezToken *t = it->data;
                if (t->type == OREZ_TEXT) {
                        GList *prev = g_list_previous(it);
                        if (prev) {
                                OrezToken *t_prev = prev->data;
                                if (t_prev->type == OREZ_SNIPPET_NAME_DELIMITER
                                    || t_prev->type == OREZ_LANGUAGE_END_MARK
                                    || t_prev->type == OREZ_SNIPPET_APPENDING_MARK
                                    || t_prev->type == OREZ_SNIPPET_PREPENDING_MARK
                                    || t_prev->type == OREZ_TAG_END_MARK) {
                                        while (1) {
                                                OrezPair *snippet_reference
                                                        = find_snippet_reference(t->content,
                                                                                 symbols->snippet_reference_beginning_mark,
                                                                                 symbols->snippet_reference_end_mark,
                                                                                 symbols->snippet_name_continuation);
                                                if (snippet_reference) {
                                                        tokens = split_snippet(tokens,
                                                                               it,
                                                                               snippet_reference,
                                                                               symbols->snippet_name_continuation,
                                                                               symbols->snippet_reference_beginning_mark,
                                                                               symbols->snippet_reference_end_mark);
                                                        free(snippet_reference);
                                                } else break;
                                        }
                                }
                        }
                }
                it = g_list_next(it);
        }
        /* 为记号列表尾部追加一个片段界限符，以便于后续程序处理 */
        GList *last_it = g_list_last(tokens);
        OrezToken *last_token = last_it->data;
        if (last_token->type != OREZ_SNIPPET_DELIMITER) {
                tokens = orez_snippet_delimiter(tokens, symbols->snippet_delimiter);
        }
        return tokens;
}
typedef enum {
        OREZ_SNIPPET,
        OREZ_SNIPPET_WITH_NAME,
        OREZ_SNIPPET_NAME,
        OREZ_SNIPPET_LANGUAGE,
        OREZ_SNIPPET_APPENDING_OPERATOR,
        OREZ_SNIPPET_PREPENDING_OPERATOR,
        OREZ_SNIPPET_TAG_REFERENCE,
        OREZ_SNIPPET_TAG,
        OREZ_SNIPPET_CONTENT,
        OREZ_SNIPPET_REFERENCE,
        OREZ_SNIPPET_TEXT
} OrezSyntaxType;
typedef struct {
        OrezSyntaxType type;
        GList *tokens;
} OrezSyntax;
static GNode *syntax_tree_init(GList *tokens)
{
        GNode *root = g_node_new(NULL);
        for (GList *it = g_list_first(tokens);
             it != NULL;
             it = g_list_next(it)) {
                OrezToken *t = it->data;
                if (t->type == OREZ_SNIPPET_DELIMITER) {
                        GList *prev = g_list_previous(it);
                        if (prev) {
                                OrezSyntax *snippet = malloc(sizeof(OrezSyntax));
                                /* 默认类型，后续会确定它是否为有名片段 */
                                snippet->type = OREZ_SNIPPET;
                                snippet->tokens = NULL;
                                GList *a = prev;
                                while (1)
                                {
                                        if (a) {
                                                OrezToken *t_a = a->data;
                                                if (t_a->type == OREZ_SNIPPET_DELIMITER) break;
                                                else {
                                                        if (t_a->type == OREZ_SNIPPET_NAME_DELIMITER) {
                                                                snippet->type = OREZ_SNIPPET_WITH_NAME;
                                                        }
                                                        snippet->tokens = g_list_prepend(snippet->tokens, t_a);
                                                }
                                        } else break;
                                        a = g_list_previous(a);
                                }
                                /* 之前在记号列表尾部强行插入了一个片段界限记号，
                                   它可能会导致语法结点的内容为空，故而需要予以忽略 */
                                if (snippet->tokens) {
                                        g_node_append_data(root, snippet);
                                } else free(snippet);
                        }
                }
        }
        /* 释放片段界限记号 */
        for (GList *it = g_list_first(tokens);
             it != NULL;
             it = g_list_next(it)) {
                OrezToken *t = it->data;
                if (t->type == OREZ_SNIPPET_DELIMITER) {
                        g_string_free(t->content, TRUE);
                        free(t);
                }
        }
        /* 销毁记号列表，它已完成使命 */
        g_list_free(tokens);
        return root;
}
static size_t first_token_line_number(GNode *x)
{
        size_t line_number = 0;
        OrezSyntax *a = x->data;
        if (a->tokens) {
                OrezToken *t = g_list_first(a->tokens)->data;
                line_number = t->line_number;
        } else {
                line_number = first_token_line_number(x->children);
        }
        return line_number;
}
static void print_syntax_tree(GNode *root)
{
        if (root) {
                if (root->data) {
                        OrezSyntax *a = root->data;
                        switch (a->type) {
                        case OREZ_SNIPPET:
                                printf("<snippet>\n");
                                break;
                        case OREZ_SNIPPET_WITH_NAME:
                                printf("<snippet with name>\n");
                                break;
                        case OREZ_SNIPPET_NAME:
                                printf("<name>\n");
                                break;
                        case OREZ_SNIPPET_LANGUAGE:
                                printf("<language>\n");
                                break;
                        case OREZ_SNIPPET_APPENDING_OPERATOR:
                                printf("<snippet appending operator>\n");
                                break;
                        case OREZ_SNIPPET_PREPENDING_OPERATOR:
                                printf("<snippet prepending operator>\n");
                                break;
                        case OREZ_SNIPPET_TAG_REFERENCE:
                                printf("<snippet tag reference>\n");
                                break;
                        case OREZ_SNIPPET_TAG:
                                printf("<snippet tag>\n");
                                break;
                        case OREZ_SNIPPET_CONTENT:
                                printf("<snippet content>\n");
                                break;
                        case OREZ_SNIPPET_REFERENCE:
                                printf("<snippet reference>\n");
                                break;
                        case OREZ_SNIPPET_TEXT:
                                printf("<snippet text>\n");
                                break;
                        default:
                                g_error("Illegal syntax in %lu.\n",
                                        first_token_line_number(root));
                        }
                        if (a->tokens) print_tokens(a->tokens);
                }
                for (GNode *p = root->children; p != NULL; p = p->next) {
                        print_syntax_tree(p);
                }
        }
}
static void delete_syntax_tree_body(GNode *root)
{
        if (root) {
                if (root->data) {
                        OrezSyntax *a = root->data;
                        if (a->tokens) delete_tokens(a->tokens);
                        free(a);
                }
                for (GNode *p = root->children; p != NULL; p = p->next) {
                        delete_syntax_tree_body(p);
                }
        }
}
static void delete_syntax_tree(GNode *root)
{
        delete_syntax_tree_body(root);
        g_node_destroy(root);
}
static GList *create_language(GNode *x, GList *it)
{
        OrezToken *token = it->data;
        if (token->type == OREZ_LANGUAGE_BEGINNING_MARK) {
                OrezSyntax *language = malloc(sizeof(OrezSyntax));
                it = g_list_next(it);
                token = it->data;
                language->type = OREZ_SNIPPET_LANGUAGE;
                language->tokens = g_list_append(NULL, token);
                g_node_append_data(x, language);
                it = g_list_next(it);
                token = it->data;
                if (token->type != OREZ_LANGUAGE_END_MARK) {
                        g_error("Line %lu: Illegal language mark!",
                                token->line_number);
                }
                it = g_list_next(it);
        }
        return it;
}
static GList *create_tag_reference(GNode *x, GList *it)
{
        OrezToken *token = it->data;
        if (token->type == OREZ_TAG_BEGINNING_MARK) {
                bool is_tag_ref = false;
                for (GList *it_a = it;
                     it_a != NULL;
                     it_a = g_list_next(it_a)) {
                        OrezToken *t = it_a->data;
                        if (t->type == OREZ_SNIPPET_APPENDING_MARK
                            || t->type == OREZ_SNIPPET_PREPENDING_MARK) {
                                is_tag_ref = true;
                                break;
                        }
                }
                if (is_tag_ref) {
                        OrezSyntax *tag_ref = malloc(sizeof(OrezSyntax));
                        it = g_list_next(it);
                        token = it->data;
                        if (token->type != OREZ_TEXT) {
                                g_error("Line %lu: Illegal tag reference.",
                                        token->line_number);
                        }
                        tag_ref->type = OREZ_SNIPPET_TAG_REFERENCE;
                        tag_ref->tokens = g_list_append(NULL, token);
                        g_node_append_data(x, tag_ref);
                        it = g_list_next(it);
                        token = it->data;
                        if (token->type != OREZ_TAG_END_MARK) {
                                g_error("Line %lu: Illegal tag reference.",
                                        token->line_number);
                        }
                        it = g_list_next(it);
                }
        }
        return it;
}
static GList *create_tag(GNode *x, GList *it)
{
        OrezToken *token = it->data;
        if (token->type == OREZ_TAG_BEGINNING_MARK) {
                OrezSyntax *tag = malloc(sizeof(OrezSyntax));
                it = g_list_next(it);
                token = it->data;
                if (token->type != OREZ_TEXT) {
                        g_error("Line %lu: Illegal tag.",
                                token->line_number);
                }
                tag->type = OREZ_SNIPPET_TAG;
                tag->tokens = g_list_append(NULL, token);
                g_node_append_data(x, tag);
                it = g_list_next(it);
                token = it->data;
                if (token->type != OREZ_TAG_END_MARK) {
                        g_error("Line %lu: Illegal tag reference.",
                                token->line_number);
                }
                it = g_list_next(it);
        }
        return it;
}
static void create_child_nodes_of_snippet_with_name(GNode *x)
{
        OrezSyntax *snippet = x->data;
        GList *it = g_list_first(snippet->tokens);
        OrezToken *token = it->data;
        /* 构造片段名字结点 */
        OrezSyntax *snippet_name = malloc(sizeof(OrezSyntax));
        snippet_name->type = OREZ_SNIPPET_NAME;
        snippet_name->tokens = g_list_append(NULL, token);
        g_node_append_data(x, snippet_name);
        /* 跳过片段名字界限符 */
        it = g_list_next(it);
        token = it->data;
        if (token->type != OREZ_SNIPPET_NAME_DELIMITER) {
                g_error("Line %lu: Illegal snippet name.", token->line_number);
        }
        it = g_list_next(it);
        /* 构造可能存在的语言和标签引用结点，
           之所以重复两次，是因 Orez 语法未区分语言和标签引用的先后 */
        it = create_language(x, it);
        it = create_tag_reference(x, it);
        it = create_language(x, it);
        it = create_tag_reference(x, it);
        /* 构造可能存在的运算符结点 */
        token = it->data;
        if (token->type == OREZ_SNIPPET_APPENDING_MARK
            || token->type == OREZ_SNIPPET_PREPENDING_MARK) {
                OrezSyntax *operator = malloc(sizeof(OrezSyntax));
                if (token->type == OREZ_SNIPPET_APPENDING_MARK) {       
                        operator->type = OREZ_SNIPPET_APPENDING_OPERATOR;
                } else {       
                        operator->type = OREZ_SNIPPET_PREPENDING_OPERATOR;
                }
                operator->tokens = g_list_append(NULL, token);
                g_node_append_data(x, operator);
                it = g_list_next(it);
        }
        /* 构造可能存在的标签结点 */
        it = create_tag(x, it);
        /* 构造有名片段的内容结点 */
        OrezSyntax *content = malloc(sizeof(OrezSyntax));
        content->type = OREZ_SNIPPET_CONTENT;
        content->tokens = NULL;
        for (GList *it_a = it; it_a != NULL; it_a = g_list_next(it_a)) {
                token = it_a->data;
                if (token->type == OREZ_SNIPPET_REFERENCE_BEGINNING_MARK
                    || token->type == OREZ_SNIPPET_REFERENCE_END_MARK
                    || token->type == OREZ_TEXT) {
                        content->tokens = g_list_append(content->tokens, token);
                } else {
                        g_error("Line %lu: Illegal snippet.",
                                token->line_number);
                }
        }
        g_node_append_data(x, content);
        /* 释放一些未进入语法树的记号 */
        for (it = g_list_first(snippet->tokens);
             it != NULL;
             it = g_list_next(it)) {
                token = it->data;
                if (token->type == OREZ_SNIPPET_NAME_DELIMITER
                    || token->type == OREZ_LANGUAGE_BEGINNING_MARK
                    || token->type == OREZ_LANGUAGE_END_MARK
                    || token->type == OREZ_TAG_BEGINNING_MARK
                    || token->type == OREZ_TAG_END_MARK) {
                        g_string_free(token->content, TRUE);
                        free(token);
                }
        }
        /* 释放 snippet 的词法表，因为其内容已全部转移至其子结点中了 */
        g_list_free(snippet->tokens);
        snippet->tokens = NULL;
}
static void create_child_nodes_of_snippet_content(GNode *x)
{
        OrezSyntax *content = x->data;
        GList *it = g_list_first(content->tokens);
        while (1) {
                if (!it) break;
                OrezToken *token = it->data;
                if (token->type == OREZ_SNIPPET_REFERENCE_BEGINNING_MARK) {
                        /* 构造片段引用 */
                        it = g_list_next(it);
                        token = it->data;
                        if (token->type != OREZ_TEXT) {
                                g_error("Line %lu: Illegal snippet reference.",
                                        token->line_number);
                        }
                        OrezSyntax *reference = malloc(sizeof(OrezSyntax));
                        reference->type = OREZ_SNIPPET_REFERENCE;
                        reference->tokens = g_list_append(NULL, token);
                        g_node_append_data(x, reference);
                        /* 验证片段引用的语法合法性 */
                        it = g_list_next(it);
                        token = it->data;
                        if (token->type != OREZ_SNIPPET_REFERENCE_END_MARK) {
                                g_error("Line %lu: Illegal snippet reference.",
                                        token->line_number);
                        }
                } else if (token->type == OREZ_TEXT) {
                        OrezSyntax *text = malloc(sizeof(OrezSyntax));
                        text->type = OREZ_SNIPPET_TEXT;
                        text->tokens = g_list_append(NULL, token);
                        g_node_append_data(x, text);
                } else {
                        g_error("Line %lu: Illegal snippet.",
                                token->line_number);
                }
                it = g_list_next(it);
        }
        /* 释放 content->tokens */
        for (it = g_list_first(content->tokens);
             it != NULL;
             it = g_list_next(it)) {
                OrezToken *token = it->data;
                if (token->type == OREZ_SNIPPET_REFERENCE_BEGINNING_MARK
                    || token->type == OREZ_SNIPPET_REFERENCE_END_MARK) {
                        g_string_free(token->content, TRUE);
                        free(token);
                }
        }
        g_list_free(content->tokens);
        content->tokens = NULL;
}
static GNode *orez_parser(GList *tokens)
{
        GNode *root = syntax_tree_init(tokens);
        for (GNode *p = root->children; p != NULL; p = p->next) {
                OrezSyntax *snippet = p->data;
                if (snippet->type == OREZ_SNIPPET_WITH_NAME) {
                        create_child_nodes_of_snippet_with_name(p);
                }
        }
        for (GNode *p = root->children; p != NULL; p = p->next) {
                OrezSyntax *a = p->data;
                if (a->type == OREZ_SNIPPET_WITH_NAME) {
                        for (GNode *q = p->children; q != NULL; q = q->next) {
                                OrezSyntax *b = q->data;
                                if (b->type == OREZ_SNIPPET_CONTENT) {
                                        create_child_nodes_of_snippet_content(q);
                                }
                        }
                }
        }
        return root;
}
typedef struct {
        GPtrArray *time_order;
        GPtrArray *spatial_order;
        GPtrArray *emissions;
} OrezTie;
static GString *compact_text(GString *a,
                             GPtrArray *useless_characters)
{
        GString *b = g_string_new(a->str);
        for (size_t i = 0; i < useless_characters->len; i++) {
                GString *c = g_ptr_array_index(useless_characters, i);
                g_string_replace(b, c->str, "", 0);
        }
        /* 去除引号 */
        g_string_replace(b, "\'", "", 0);
        g_string_replace(b, "\"", "", 0);
        /* 如有需要，可继续添加 */
        /* ... ... ... */
        return b;
}
static void init_tie(GHashTable *relations,
                     GNode *x,
                     GPtrArray *useless_characters)
{
        GString *name = NULL;
        for (GNode *p = x->children; p != NULL; p = p->next) {
                OrezSyntax *a = p->data;
                if (a->type == OREZ_SNIPPET_NAME) {
                        OrezToken *t = g_list_first(a->tokens)->data;
                        name = t->content;
                        break;
                }
        }
        if (!name) {
                g_error("Line %lu: Illegal snippet.", first_token_line_number(x));
        }
        GString *key = compact_text(name, useless_characters);
        OrezTie *tie = g_hash_table_lookup(relations, key);
        if (tie) {
                bool has_appending_operator = FALSE;
                bool has_prepending_operator = FALSE;
                GString *tag_reference = NULL;
                for (GNode *p = x->children; p != NULL; p = p->next) {
                        OrezSyntax *a = p->data;
                        if (a->type == OREZ_SNIPPET_TAG_REFERENCE) {
                                OrezToken *t_a = g_list_first(a->tokens)->data;
                                tag_reference = t_a->content;
                        }
                        if (a->type == OREZ_SNIPPET_APPENDING_OPERATOR) {
                                has_appending_operator = TRUE;
                                break;
                        }
                        if (a->type == OREZ_SNIPPET_PREPENDING_OPERATOR) {
                                has_prepending_operator = TRUE;
                                break;
                        }
                }
                if (tag_reference) {
                        GString *t = compact_text(tag_reference, useless_characters);
                        int id = -1;
                        for (int i = 0; i < tie->time_order->len; i++) {
                                GNode *y = g_ptr_array_index(tie->time_order, i);
                                for (GNode *it = y->children; it != NULL; it = it->next) {
                                        OrezSyntax *a = it->data;
                                        if (a->type == OREZ_SNIPPET_TAG) {
                                                OrezToken *tag_token = g_list_first(a->tokens)->data;
                                                GString *s = compact_text(tag_token->content,
                                                                          useless_characters);
                                                if (g_string_equal(s, t)) {
                                                        id = i;
                                                        g_string_free(s, TRUE);
                                                        break;
                                                }
                                                g_string_free(s, TRUE);
                                        }
                                }
                                if (id >= 0) break;
                        }
                        if (id < 0) {
                                g_error("Line %lu: The referenced tag does not exist.",
                                        first_token_line_number(x));
                        } else {
                                if (has_appending_operator) {
                                        g_ptr_array_insert(tie->time_order, id + 1, x);
                                } else if (has_prepending_operator) {
                                        g_ptr_array_insert(tie->time_order, id, x);
                                } else {
                                        g_error("Line %lu: The snippet needs an operator.",
                                                 first_token_line_number(x));
                                }
                        }
                        g_string_free(t, TRUE);
                } else {
                        if (has_appending_operator) {
                                g_ptr_array_add(tie->time_order, x);
                        } else if (has_prepending_operator) {
                                g_ptr_array_insert(tie->time_order, 0, x);
                        } else {
                                g_error("Line %lu: The snippet needs an operator.",
                                        first_token_line_number(x));
                        }
                }
                g_ptr_array_add(tie->spatial_order, x);
                g_string_free(key, TRUE);
        } else {
                tie = malloc(sizeof(OrezTie));
                tie->time_order = g_ptr_array_new();
                tie->spatial_order = g_ptr_array_new();
                tie->emissions = g_ptr_array_new();
                g_ptr_array_add(tie->time_order, x);
                g_ptr_array_add(tie->spatial_order, x);
                if (!g_hash_table_insert(relations, key, tie)) {
                        g_error("Line %lu: Failed to insert snippet named <%s>"
                                " into the relation table.",
                                first_token_line_number(x), key->str);
                }
        }
}
static void create_relation(GHashTable *relations,
                            GNode *u,
                            OrezSyntax *a,
                            GPtrArray *useless_characters)
{
        OrezToken *t = g_list_first(a->tokens)->data;
        GString *s = compact_text(t->content, useless_characters);
        OrezTie *tie = g_hash_table_lookup(relations, s);
        if (tie) {
                g_ptr_array_add(tie->emissions, u);
        } else {
                g_error("Line %lu: The snippet named <%s> never existed.",
                        t->line_number, t->content->str);
        }
        g_string_free(s, TRUE);
}
static GHashTable *orez_create_relations(GNode *root, GPtrArray *useless_characters)
{
        GHashTable *relations = g_hash_table_new((GHashFunc)g_string_hash,
                                                 (GEqualFunc)g_string_equal);
        for (GNode *it = root->children; it != NULL; it = it->next) {
                OrezSyntax *a = it->data;
                if (a->type != OREZ_SNIPPET_WITH_NAME) continue;
                init_tie(relations, it, useless_characters);
        }
        for (GNode *it = root->children; it != NULL; it = it->next) {
                OrezSyntax *a = it->data;
                if (a->type != OREZ_SNIPPET_WITH_NAME) continue;
                GNode *body = g_node_last_child(it);
                for (GNode *o_it = body->children; o_it != NULL; o_it = o_it->next) {
                        OrezSyntax *b = o_it->data;
                        if (b->type != OREZ_SNIPPET_REFERENCE) continue;
                        create_relation(relations, it, b, useless_characters);
                }
        }
        return relations;
}
static void orez_destroy_relations(GHashTable *relations)
{
        GList *keys = g_hash_table_get_keys(relations);
        GList *it = keys;
        while (it) {
                GString *key = it->data;
                OrezTie *tie = g_hash_table_lookup(relations, key);
                g_ptr_array_free(tie->spatial_order, TRUE);
                g_ptr_array_free(tie->time_order, TRUE);
                g_ptr_array_free(tie->emissions, TRUE);
                free(tie);
                g_string_free(key, TRUE);
                it = it->next;
        }
        g_list_free(keys);
        g_hash_table_destroy(relations);
}
static GString *cascade_indents(GList *indents)
{
        GString *current_indent = g_string_new(NULL);
        for (GList *it = indents; it != NULL; it = it->next) {
                GString *a = it->data;
                g_string_append(current_indent, a->str);
        }
        return current_indent;
}
static char *squeeze_head(GString *text)
{
        char *p = text->str;
        while (1) {
                if (*p == ' ' || *p == '\t') p++;
                else if ((unsigned char)*p == 0xE3) {
                        char *q = p + 1;
                        if ((unsigned char)*q == 0x80) {
                                char *r = q + 1;
                                if ((unsigned char)*r == 0x80) {
                                        p = r + 1;
                                } else break;
                        } else break;
                } else if (*p == '\n') {
                        p++;
                        break;
                } else break;
        }
        return p;
}
static char *squeeze_tail(GString *text)
{
        char *p = text->str + text->len - 1;
        while (1) {
                if (p == text->str) break;
                else if (*p == ' ' || *p == '\t') p--;
                else if ((unsigned char)*p == 0x80) {
                        char *q = p - 1;
                        if ((unsigned char)*q == 0x80) {
                                char *r = q - 1;
                                if ((unsigned char)*r == 0xE3) {
                                        p = r - 1;
                                } else break;
                        } else break;
                } else if (*p == '\n') {
                        break;
                } else break;
        }
        return p;
}
static GString *take_last_blank_line(OrezSyntax *a)
{
        assert(a);
        assert(a->type == OREZ_SNIPPET_TEXT);
        GString *blank_line = g_string_new(NULL);
        OrezToken *t = g_list_first(a->tokens)->data;
        if (t->content->len <= 1) return blank_line;
        else {
                char *p = squeeze_tail(t->content);
                if (p < t->content->str + t->content->len) {
                        for (char *q = p + 1; *q != '\0'; q++) {
                                g_string_append_c(blank_line, *q);
                        }
                }
        }
        return blank_line;
}
static void orez_tangle(const char *input_file_name,
                        GHashTable *relations,
                        GString *entrance,
                        GPtrArray *useless_characters,
                        GList **indents, /* 用于记录所引用的片段的缩进层次 */
                        bool show_line_number, /* 用于控制输出的内容是否包含行号 */
                        FILE *output)
{
        GString *key = compact_text(entrance, useless_characters);
        OrezTie *tie = g_hash_table_lookup(relations, key);
        if (!tie) {
                g_error("Snippet <%s> never existed!", entrance->str);
        }
        for (size_t i = 0; i < tie->time_order->len; i++) {
                GNode *snippet = g_ptr_array_index(tie->time_order, i);
                GNode *body = g_node_last_child(snippet);
                for (GNode *it = body->children; it != NULL; it = it->next) {
                        OrezSyntax *a = it->data;
                        OrezToken *ta = g_list_first(a->tokens)->data;
                        GString *text = ta->content;
                        if (a->type == OREZ_SNIPPET_TEXT) {
                                GString *cache = g_string_new(NULL);
                                GString *current_indent = cascade_indents(*indents); /* 将各层次的缩进串联起来 */
                                if (show_line_number) {
                                        GString *line_number = g_string_new(NULL);
                                        g_string_printf(line_number,
                                                        "#line %zu \"%s\"\n",
                                                        first_token_line_number(it), input_file_name);
                                        g_string_append(cache, line_number->str);
                                        g_string_free(line_number, TRUE);
                                
                                }
                                char *body_head = squeeze_head(text);
                                char *body_tail = squeeze_tail(text);
                                if (body_head < body_tail) {/* 忽略空白段 */
                                        g_string_append(cache, current_indent->str);
                                        for (char *p = body_head; p != body_tail; p++) {
                                                g_string_append_c(cache, *p);
                                                if (*p == '\n') {
                                                        g_string_append(cache, current_indent->str);
                                                }
                                        }
                                        fputs(cache->str, output);
                                        fputs("\n", output);
                                }
                                g_string_free(current_indent, TRUE);
                                g_string_free(cache, TRUE);
                        } else if (a->type == OREZ_SNIPPET_REFERENCE) {
                                GNode *prev_it = g_node_prev_sibling(it);
                                if (prev_it) {
                                        OrezSyntax *prev_a = prev_it->data;
                                        if (prev_a->type == OREZ_SNIPPET_TEXT) {
                                                GString *blank_line = take_last_blank_line(prev_a);
                                                *indents = g_list_prepend(*indents, blank_line);
                                        }
                                        OrezToken *ta = g_list_first(a->tokens)->data;
                                        orez_tangle(input_file_name, relations,
                                                    ta->content, useless_characters,
                                                    indents, show_line_number, output);
                                        /* 删除当前层次的缩进 */
                                        g_string_free((*indents)->data, TRUE);
                                        *indents = g_list_delete_link(*indents, *indents);
                                }
                        } else {
                                g_error("Line %lu: There is an undefined type!",
                                        first_token_line_number(it));
                        }
                }
        }
        g_string_free(key, TRUE);
}
static GList *get_thread(GHashTable *relations,
                         GString *entrance,
                         GPtrArray *useless_characters,
                         GList *thread)
{
        GString *key = compact_text(entrance, useless_characters);
        OrezTie *tie = g_hash_table_lookup(relations, key);
        for (size_t i = 0; i < tie->spatial_order->len; i++) {
                GNode *t = g_ptr_array_index(tie->spatial_order, i);
                GNode *x = g_node_last_child(t);
                thread = g_list_prepend(thread, t);
                for (GNode *it = x->children; it != NULL; it = it->next) {
                        OrezSyntax *a = it->data;
                        if (a->type == OREZ_SNIPPET_REFERENCE) {
                                OrezToken *ta = g_list_first(a->tokens)->data;
                                thread = get_thread(relations,
                                                    ta->content,
                                                    useless_characters,
                                                    thread);
                        }
                }
        }
        g_string_free(key, TRUE);
        return thread;
}
static OrezToken *find_language_token(GNode *x)
{
        OrezToken *language_token = NULL;
        for (GNode *p = x->children; p != NULL; p = p->next) {
                OrezSyntax *a = p->data;
                if (a->type == OREZ_SNIPPET_LANGUAGE) {
                        language_token = g_list_first(a->tokens)->data;
                        break;
                }
        }
        return language_token;
}
static void spread_language_mark_in_thread(GList *thread)
{
        OrezToken *language_token = NULL;
        for (GList *it = thread; it != NULL; it = it->next) {
                language_token = find_language_token(it->data);
                if (language_token) break;
        }
        if (!language_token) return;
        for (GList *it = thread; it != NULL; it = it->next) {
                GNode *t = it->data;
                /* 检测 t 是否含有语言标记 */
                OrezToken *language_token_a = find_language_token(t);;
                if (language_token_a) {
                        if (!g_string_equal(language_token->content,
                                            language_token_a->content)) {
                                g_error("Line %lu and %lu: "
                                        "there are two different language marks.",
                                        language_token->line_number,
                                        language_token_a->line_number);
                        }
                } else {
                        OrezToken *new_language_token = malloc(sizeof(OrezToken));
                        new_language_token->type = OREZ_TEXT;
                        new_language_token->content = g_string_new(language_token->content->str);
                        /* 确定语言标记的行号 */
                        OrezSyntax *name = g_node_first_child(t)->data;
                        OrezToken *name_token = g_list_first(name->tokens)->data;
                        size_t line_number = name_token->line_number;
                        for (char *p = name_token->content->str; *p != '\0'; p++) {
                                if (*p == '\n') line_number++;
                        }
                        new_language_token->line_number = line_number;
                        /* 构造语法结点 */
                        OrezSyntax *new_language = malloc(sizeof(OrezSyntax));
                        new_language->type = OREZ_SNIPPET_LANGUAGE;
                        new_language->tokens = g_list_append(NULL, new_language_token);
                        g_node_insert_data_after(t, g_node_first_child(t), new_language);
                }
        }
}
static void spread_language_mark(GNode *root,
                                 GHashTable *relations,
                                 GPtrArray *useless_characters)
{
        for (GNode *it = root->children; it != NULL; it = it->next) {
                OrezSyntax *a = it->data;
                if (a->type == OREZ_SNIPPET_WITH_NAME) {
                        OrezSyntax *b = g_node_first_child(it)->data;
                        OrezToken *token_b = g_list_first(b->tokens)->data;
                        GList *thread = get_thread(relations,
                                                   token_b->content,
                                                   useless_characters,
                                                   NULL);
                        spread_language_mark_in_thread(thread);
                        g_list_free(thread);
                } 
        }
        /* 检测 */
        for (GNode *it = root->children; it != NULL; it = it->next) {
                OrezSyntax *a = it->data;
                if (a->type == OREZ_SNIPPET_WITH_NAME) {
                        OrezToken *language_token = find_language_token(it);
                        if (!language_token){
                                OrezSyntax *name = g_node_first_child(it)->data;
                                OrezToken *name_token = g_list_first(name->tokens)->data;
                                g_warning("Line %lu: "
                                          "the language of the snippet <%s> is unknown.",
                                          name_token->line_number,
                                          name_token->content->str);
                        }
                } 
        }
}
/* 消除 text 的前冗白。注意：text 会被释放。*/
static GString *orez_string_chug(GString *text, GPtrArray *useless_characters)
{
        if (text->len == 0) return text;
        GString *new_text = g_string_new(NULL);
        /* 构造字符串闭区间 [a, b] */
        char *a = text->str, *b = text->str + text->len - 1;
        char *p = a;
        while (1) {
                char *t = p;
                for (size_t i = 0; i < useless_characters->len; i++) {
                        GString *c = g_ptr_array_index(useless_characters, i);
                        char *q = p;
                        if (am_i_here(p, b, &q, c->str, 1)) p = q + 1;
                }
                if (t == p) break;
        }
        for ( ; *p != '\0'; p++) g_string_append_c(new_text, *p);
        g_string_free(text, TRUE);
        return new_text;
}
/* 消除 text 的后冗白。注意：text 会被释放。*/
static GString *orez_string_chomp(GString *text, GPtrArray *useless_characters)
{
        if (text->len == 0) return text;
        /* 构造字符串闭区间 [a, b] */
        char *a = text->str, *b = text->str + text->len - 1;
        GString *new_text = g_string_new(NULL);
        char *p = b;
        while (1) {
                char *t = p;
                for (size_t i = 0; i < useless_characters->len; i++) {
                        GString *c = g_ptr_array_index(useless_characters, i);
                        char *q = p;
                        if (am_i_here(a, p, &q, c->str, -1)) {
                                p = q - 1;
                        }
                }
                if (t == p /* 走不动了 */
                    || p < a /* 走过头了 */ ) break;
        }
        p++;
        for (char *q = text->str; q != p; q++) {
                g_string_append_c(new_text, *q);
        }
        g_string_free(text, TRUE);
        return new_text;
}
/* 消除 text 的前后冗白。注意：text 会被释放。*/
static GString *orez_string_strip(GString *text, GPtrArray *useless_characters)
{
        GString *new_text = orez_string_chomp(orez_string_chug(text,
                                                               useless_characters),
                                              useless_characters);
        return new_text;
}
static void strip_snippet(GNode *x, GPtrArray *useless_characters)
{
        OrezSyntax *a = x->data;
        OrezToken *t_a = g_list_first(a->tokens)->data;
        t_a->content = orez_string_strip(t_a->content, useless_characters);
}
static void strip_snippet_with_name(GNode *x, GPtrArray *useless_characters)
{
        for (GNode *it = x->children; it != NULL; it = it->next) {
                OrezSyntax *a = it->data;
                if (a->type == OREZ_SNIPPET_CONTENT) {
                        OrezSyntax *b = NULL;
                        OrezToken *t_b = NULL;
                        for (GNode *p = it->children; p != NULL; p = p->next) {
                                b = p->data;
                                t_b = g_list_first(b->tokens)->data;
                                if (b->type == OREZ_SNIPPET_REFERENCE) {
                                        t_b->content = orez_string_strip(t_b->content, useless_characters);
                                }
                        }
                        if (b->type == OREZ_SNIPPET_TEXT) {
                                t_b->content = orez_string_chomp(t_b->content, useless_characters);
                        } else {
                                g_warning("Line %lu: Illegal snippet.", t_b->line_number);
                        }
                } else {
                        OrezSyntax *b = it->data;
                        OrezToken *t_b = g_list_first(b->tokens)->data;
                        t_b->content = orez_string_strip(t_b->content,
                                                         useless_characters);
                }
        }
}
static void strip_all_snippets(GNode *root,
                               GPtrArray *useless_characters)
{
        for (GNode *it = root->children; it != NULL; it = it->next) {
                OrezSyntax *a = it->data;
                if (a->type == OREZ_SNIPPET) {
                        strip_snippet(it, useless_characters);
                } else {
                        strip_snippet_with_name(it, useless_characters);
                }
        }
}
static void append_yaml_string(GString *cache, char *text, const char *indent, char *padding)
{
        /* 处理多行字串的缩进 */
        for (char *p = text; *p != '\0'; p++) {
                g_string_append_c(cache, *p);
                if (*p == '\n') {
                        g_string_append(cache, indent);
                        if (padding) g_string_append(cache, padding);
                }
        }
}
static GList *output_snippet(GNode *x, GList *yaml)
{
        OrezSyntax *snippet = x->data;
        OrezToken *snippet_token = g_list_first(snippet->tokens)->data;
        GString *cache = g_string_new("- type: 0\n");
        g_string_append(cache, "  content: |-\n");
        /* 缩进 4 个空格，增加左引号 */
        g_string_append(cache, "    '");
         /* 缩进 4 个空格 */
        append_yaml_string(cache, snippet_token->content->str, "    ", NULL);
        g_string_append(cache, "'\n"); /* 增加右引号 */
        return g_list_append(yaml, cache);
}
static GList *output_snippet_with_name(GNode *x,
                                       GHashTable *relations,
                                       GPtrArray *useless_characters,
                                       char *snippet_reference_padding,
                                       GList *yaml)
{
        OrezSyntax *name = g_node_first_child(x)->data;
        OrezToken *name_token = g_list_first(name->tokens)->data;
        GString *x_tie_key = compact_text(name_token->content, useless_characters);
        OrezTie *x_tie = g_hash_table_lookup(relations, x_tie_key);
        GString *cache = g_string_new("- type: 1\n");
        for (GNode *it = x->children; it != NULL; it = it->next) {
                OrezSyntax *a = it->data;
                switch (a->type) {
                case OREZ_SNIPPET_NAME:
                        do {
                                /* 片段名 */
                                OrezToken *t = g_list_first(a->tokens)->data;
                                g_string_append(cache, "  name: |-\n");
                                g_string_append(cache, "    '");
                                append_yaml_string(cache, t->content->str, "    ", NULL);
                                g_string_append(cache, "'\n");
                                /* 片段名的 hash 值 */
                                g_string_append_printf(cache, "  hash: '%s'\n", x_tie_key->str);
                                /* 确定 id */
                                if (x_tie->spatial_order->len > 1) {
                                        size_t id = 0;
                                        for (size_t i = 0; i < x_tie->spatial_order->len; i++) {
                                                if (x == g_ptr_array_index(x_tie->spatial_order, i)) {
                                                        id = i;
                                                        break;
                                                }
                                        }
                                        g_string_append_printf(cache, "  id: %lu\n", id + 1);
                                }
                        } while (0);
                        break;
                case OREZ_SNIPPET_LANGUAGE:
                        do {
                                OrezToken *t = g_list_first(a->tokens)->data;
                                g_string_append_printf(cache, "  language: '%s'\n", t->content->str);
                        } while (0);
                        break;
                case OREZ_SNIPPET_TAG_REFERENCE:
                        do {
                                OrezToken *t = g_list_first(a->tokens)->data;
                                g_string_append(cache, "  tag_reference: \n");
                                g_string_append(cache, "    name: |-\n");
                                g_string_append(cache, "      '");
                                append_yaml_string(cache, t->content->str, "       ", NULL);
                                g_string_append(cache, "'\n");
                                /* 输出 hash */
                                GString *tag_reference_hash = compact_text(t->content, useless_characters);
                                g_string_append_printf(cache, "    hash: '%s'\n", tag_reference_hash->str);
                                g_string_free(tag_reference_hash, TRUE);
                        } while (0);
                        break;
                case OREZ_SNIPPET_PREPENDING_OPERATOR:
                        do {
                                OrezToken *t = g_list_first(a->tokens)->data;
                                g_string_append_printf(cache, "  operator: '%s'\n", t->content->str);
                        } while (0);
                        break;
                case OREZ_SNIPPET_APPENDING_OPERATOR:
                        do {
                                OrezToken *t = g_list_first(a->tokens)->data;
                                g_string_append_printf(cache, "  operator: '%s'\n", t->content->str);
                        } while (0);
                        break;
                case OREZ_SNIPPET_TAG:
                        do {
                                OrezToken *t = g_list_first(a->tokens)->data;
                                g_string_append(cache, "  tag: \n");
                                g_string_append(cache, "    name: |-\n");
                                g_string_append(cache, "      '");
                                append_yaml_string(cache, t->content->str, "       ", NULL);
                                g_string_append(cache, "'\n");
                                /* 输出 hash */
                                GString *tag_hash = compact_text(t->content, useless_characters);
                                g_string_append_printf(cache, "    hash: '%s'\n", tag_hash->str);
                                g_string_free(tag_hash, TRUE);
                        } while (0);
                        break;
                case OREZ_SNIPPET_CONTENT:
                        do {
                                g_string_append(cache, "  content: \n");
                                for (GNode *p = it->children; p != NULL; p = p->next) {
                                        OrezSyntax *b = p->data;
                                        OrezToken *t_b = g_list_first(b->tokens)->data;
                                        if (b->type == OREZ_TEXT) {
                                                if (t_b->content->len > 0) {
                                                        g_string_append(cache, "    - text: |-\n");
                                                        g_string_append(cache, "        '");
                                                        append_yaml_string(cache,
                                                                           t_b->content->str,
                                                                           "        ", NULL);
                                                        g_string_append(cache, "'\n");
                                                }
                                        } else { /* 片段引用 */
                                                GString *y_tie_key = compact_text(t_b->content, useless_characters);
                                                OrezTie *y_tie = g_hash_table_lookup(relations, y_tie_key);
                                                g_string_append(cache, "    - reference: \n");
                                                g_string_append(cache, "        name: |-\n");
                                                g_string_append(cache, "          '");
                                                append_yaml_string(cache,
                                                                   t_b->content->str,
                                                                   "          ",
                                                                   snippet_reference_padding);
                                                g_string_append(cache, "'\n");
                                                g_string_append_printf(cache, "        hash: '%s'\n", y_tie_key->str);
                                                /* 输出引用片段的所有 ID */
                                                if (y_tie->spatial_order->len > 1) {
                                                        g_string_append(cache, "        ids: \n");
                                                        for (size_t i = 0; i < y_tie->spatial_order->len; i++) {
                                                                g_string_append_printf(cache,
                                                                                       "          - %lu\n",
                                                                                       i + 1);
                                                        }
                                                }
                                                g_string_free(y_tie_key, TRUE);
                                        }
                                }
                        } while (0);
                        break;
                default:
                        printf("Line %lu: Unknown syntax in snippet <%s>.\n",
                               first_token_line_number(g_node_first_child(x)),
                               name_token->content->str);
                }
        }
        if (x_tie->emissions->len > 0) {
                g_string_append(cache, "  emissions: \n");
                for (size_t i = 0; i < x_tie->emissions->len; i++) {
                        GNode *e = g_ptr_array_index(x_tie->emissions, i);
                        OrezSyntax *b = g_node_first_child(e)->data;
                        OrezToken *t_b = g_list_first(b->tokens)->data;
                        GString *y_tie_key = compact_text(t_b->content, useless_characters);
                        OrezTie *y_tie = g_hash_table_lookup(relations, y_tie_key);
                        g_string_append(cache, "    - emission: \n");
                        g_string_append(cache, "        name: |-\n");
                        g_string_append(cache, "          '");
                        append_yaml_string(cache, t_b->content->str, "          ", NULL);
                        g_string_append(cache, "'\n");
                        g_string_append_printf(cache, "        hash: '%s'\n", y_tie_key->str);
                        /* 确定 id */
                        if (y_tie->spatial_order->len > 1) {
                                for (size_t j = 0; j < y_tie->spatial_order->len; j++) {
                                        if (e == g_ptr_array_index(y_tie->spatial_order, j)) {
                                                g_string_append_printf(cache,
                                                                       "        id: %lu\n",
                                                                       j + 1);
                                                break;
                                        }
                                }
                        }
                        g_string_free(y_tie_key, TRUE);
                }
        }
        g_string_free(x_tie_key, TRUE);
        return g_list_append(yaml, cache);
}
static OrezSymbols *orez_create_symbols(const char *configure_file_name)
{
        OrezConfig config = {
                .snippet_delimiter = "@",
                .snippet_name_delimiter = "#",
                .snippet_name_continuation = "\\",
                .language_beginning_mark = "[",
                .language_end_mark = "]",
                .snippet_appending_mark = "+",
                .snippet_prepending_mark = "^+",
                .tag_beginning_mark = "<",
                .tag_end_mark = ">",
                .snippet_reference_beginning_mark = "#",
                .snippet_reference_end_mark = "@"
        };
        if (configure_file_name) {
                const cyaml_schema_field_t top_mapping_schema[] = {
                        CYAML_FIELD_STRING_PTR("snippet_delimiter",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, snippet_delimiter,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("snippet_name_delimiter",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, snippet_name_delimiter,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("snippet_name_continuation",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, snippet_name_continuation,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("language_beginning_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, language_beginning_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("language_end_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, language_end_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("snippet_appending_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, snippet_appending_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("snippet_prepending_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, snippet_prepending_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("tag_beginning_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, tag_beginning_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("tag_end_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, tag_end_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("snippet_reference_beginning_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, snippet_reference_beginning_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_STRING_PTR("snippet_reference_end_mark",
                                               CYAML_FLAG_POINTER|CYAML_FLAG_OPTIONAL,
                                               OrezSymbols, snippet_reference_end_mark,
                                               0, CYAML_UNLIMITED),
                        CYAML_FIELD_END
                };
                const cyaml_schema_value_t top_schema = {
                        CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, OrezConfig, top_mapping_schema),
                };
                const cyaml_config_t cyaml_config = {
                        .log_fn = cyaml_log,            /* Use the default logging function. */
                        .mem_fn = cyaml_mem,            /* Use the default memory allocator. */
                        .log_level = CYAML_LOG_WARNING, /* Logging errors and warnings only. */
                };
                OrezConfig *user_config;
                cyaml_err_t err = cyaml_load_file(configure_file_name,
                                                  &cyaml_config,
                                                  &top_schema,
                                                  (cyaml_data_t **)&user_config,
                                                  NULL);
                if (err != CYAML_OK) {
                        fprintf(stderr, "Failed to open config file!\n");
                        exit(EXIT_FAILURE);
                }
                if (user_config) {
                        if (user_config->snippet_delimiter) {
                                config.snippet_delimiter
                                        = user_config->snippet_delimiter;
                        }
                        if (user_config->snippet_name_delimiter) {
                                config.snippet_name_delimiter
                                        = user_config->snippet_name_delimiter;
                        }
                        if (user_config->snippet_name_continuation) {
                                config.snippet_name_continuation
                                        = user_config->snippet_name_continuation;
                        }
                        if (user_config->language_beginning_mark) {
                                config.language_beginning_mark
                                        = user_config->language_beginning_mark;
                        }
                        if (user_config->language_end_mark) {
                                config.language_end_mark
                                        = user_config->language_end_mark;
                        }
                        if (user_config->snippet_appending_mark) {
                                config.snippet_appending_mark
                                        = user_config->snippet_appending_mark;
                        }
                        if (user_config->snippet_prepending_mark) {
                                config.snippet_prepending_mark
                                        = user_config->snippet_prepending_mark;
                        }
                        if (user_config->tag_beginning_mark) {
                                config.tag_beginning_mark
                                        = user_config->tag_beginning_mark;
                        }
                        if (user_config->tag_end_mark) {
                                config.tag_end_mark
                                        = user_config->tag_end_mark;
                        }
                        if (user_config->snippet_reference_beginning_mark) {
                                config.snippet_reference_beginning_mark
                                        = user_config->snippet_reference_beginning_mark;
                        }
                        if (user_config->snippet_reference_end_mark) {
                                config.snippet_reference_end_mark
                                        = user_config->snippet_reference_end_mark;
                        }
                }
                OrezSymbols *symbols = malloc(sizeof(OrezSymbols));
                *symbols = (OrezSymbols) {
                        g_string_new(config.snippet_delimiter),
                        g_string_new(config.snippet_name_delimiter),
                        g_string_new(config.snippet_name_continuation),
                        g_string_new(config.language_beginning_mark),
                        g_string_new(config.language_end_mark),
                        g_string_new(config.snippet_appending_mark),
                        g_string_new(config.snippet_prepending_mark),
                        g_string_new(config.tag_beginning_mark),
                        g_string_new(config.tag_end_mark),
                        g_string_new(config.snippet_reference_beginning_mark),
                        g_string_new(config.snippet_reference_end_mark)
                };
                cyaml_free(&cyaml_config, &top_schema, user_config, 0);
                return symbols;
        } else {
                OrezSymbols *symbols = malloc(sizeof(OrezSymbols));
                *symbols = (OrezSymbols) {
                        g_string_new(config.snippet_delimiter),
                        g_string_new(config.snippet_name_delimiter),
                        g_string_new(config.snippet_name_continuation),
                        g_string_new(config.language_beginning_mark),
                        g_string_new(config.language_end_mark),
                        g_string_new(config.snippet_appending_mark),
                        g_string_new(config.snippet_prepending_mark),
                        g_string_new(config.tag_beginning_mark),
                        g_string_new(config.tag_end_mark),
                        g_string_new(config.snippet_reference_beginning_mark),
                        g_string_new(config.snippet_reference_end_mark)
                };
                return symbols;
        }
}
gboolean orez_tangle_mode = FALSE;
gboolean orez_weave_mode = FALSE;
gboolean orez_show_line_number = FALSE;
char *orez_configure = NULL;
char *orez_entrance = NULL;
char *orez_output = NULL;
char *orez_separator = NULL;
static GOptionEntry orez_entries[] = {
        {"config", 'c', 0, G_OPTION_ARG_STRING, &orez_configure,
         "Read configure file.", "<file name>"},
        {"tangle", 't', 0, G_OPTION_ARG_NONE, &orez_tangle_mode,
         "use tangle.", NULL},
        {"line", 'l', 0, G_OPTION_ARG_NONE, &orez_show_line_number,
         "Show line number when using tangle.", NULL},
        {"weave", 'w', 0, G_OPTION_ARG_NONE, &orez_weave_mode, "use weave.", NULL},
        {"entrance", 'e', 0, G_OPTION_ARG_STRING, &orez_entrance,
         "Set <snippet name> as the entrance for tangle.", "<snippet name>"},
        {"output", 'o', 0, G_OPTION_ARG_STRING, &orez_output,
         "Send output into <file>.", "<file name>"},
        {"separator", 's', 0, G_OPTION_ARG_STRING, &orez_separator,
         "Set the separator for multi-entrances and outputs.", "<character>"},
        {NULL}
};
int main(int argc, char **argv)
{
        setlocale(LC_ALL, "");
        GOptionContext *option_context = g_option_context_new("FILE");
        g_option_context_add_main_entries(option_context, orez_entries, NULL);
        if (!g_option_context_parse(option_context, &argc, &argv, NULL)) return -1;
        if (argv[1] == NULL) g_error("You should provide an orez file!\n");
        if (orez_tangle_mode && orez_weave_mode) {
                g_error("The tangle and weave modes can not be turned on simultaneously!");
        }
        if (orez_tangle_mode) {
                if (!orez_entrance) {
                        g_error("You should provided the start of thread to be tangled");
                }
        }
        char *input_file_name = argv[1]; /* main 函数的参数列表经上述过程截取后剩下的部分 */
        OrezSymbols *symbols = orez_create_symbols(orez_configure);
        GList *tokens = orez_lexer(input_file_name, symbols);
        GNode *root = orez_parser(tokens);
        GPtrArray *useless_characters = g_ptr_array_new();
        g_ptr_array_add(useless_characters, g_string_new(" "));
        g_ptr_array_add(useless_characters, g_string_new("　"));
        g_ptr_array_add(useless_characters, g_string_new("\t"));
        g_ptr_array_add(useless_characters, g_string_new("\n"));
        GHashTable *relations = orez_create_relations(root, useless_characters);
        if (orez_tangle_mode) {
                GPtrArray *entrances = g_ptr_array_new();
                GPtrArray *file_names = g_ptr_array_new();
                char *u = orez_output ? orez_output : orez_entrance;
                if (!orez_separator) {
                        GString *t = g_string_new(u);
                        g_ptr_array_add(entrances, g_string_new(orez_entrance));
                        g_ptr_array_add(file_names, compact_text(t, useless_characters));
                        g_string_free(t, TRUE);
                } else {
                        gchar **cf_names = g_strsplit(orez_entrance, orez_separator, 0);
                        gchar **v = g_strsplit(u, orez_separator, 0);
                        guint m = 0; for (gchar **s = cf_names; *s != NULL; s++) m++;
                        guint n = 0; for (gchar **s = v; *s != NULL; s++) n++;
                        assert(m == n);
                        for (guint i = 0; i < m; i++) {
                                GString *t = g_string_new(v[i]);
                                g_ptr_array_add(entrances, g_string_new(cf_names[i]));
                                g_ptr_array_add(file_names, compact_text(t, useless_characters));
                                g_string_free(t, TRUE);
                        }
                        g_strfreev(v);
                        g_strfreev(cf_names);
                }
                for (guint i = 0; i < file_names->len; i++) {
                        GString *entrance = g_ptr_array_index(entrances, i);
                        GString *file_name = g_ptr_array_index(file_names, i);
                        FILE *output = fopen(file_name->str, "w");
                        GList *indents = NULL;
                        orez_tangle(input_file_name,
                                    relations,
                                    entrance,
                                    useless_characters,
                                    &indents,
                                    orez_show_line_number,
                                    output);
                        fclose(output);
                        g_string_free(file_name, TRUE);
                        g_string_free(entrance, TRUE);
                }
                g_ptr_array_free(file_names, TRUE);
                g_ptr_array_free(entrances, TRUE);
        }
        if (orez_weave_mode) {
                spread_language_mark(root, relations, useless_characters);
                strip_all_snippets(root, useless_characters);
                GString *snippet_reference_padding
                        = make_padding(symbols->snippet_delimiter->len);
                GList *yaml = NULL;
                for (GNode *it = root->children; it != NULL; it = it->next) {
                        OrezSyntax *a = it->data;
                        if (a->type == OREZ_SNIPPET) {
                                yaml = output_snippet(it, yaml);
                        } else {
                                yaml = output_snippet_with_name(it,
                                                                relations,
                                                                useless_characters,
                                                                snippet_reference_padding->str,
                                                                yaml);
                        }
                }
                FILE *output = NULL;
                if (orez_output) output = fopen(orez_output, "w");
                for (GList *it = yaml; it != NULL; it = it->next) {
                        GString *a = it->data;
                        if (output) fprintf(output, "%s", a->str);
                        else printf("%s", a->str);
                        g_string_free(a, TRUE);
                }
                g_list_free(yaml);
                if (output) fclose(output);
                g_string_free(snippet_reference_padding, TRUE);
        }
        orez_destroy_relations(relations);
        for (size_t i = 0; i < useless_characters->len; i++) {
                GString *s = g_ptr_array_index(useless_characters, i);
                g_string_free(s, TRUE);
        }
        g_ptr_array_free(useless_characters, TRUE);
        delete_syntax_tree(root);
        g_string_free(symbols->snippet_delimiter, TRUE);
        g_string_free(symbols->snippet_name_delimiter, TRUE);
        g_string_free(symbols->snippet_name_continuation, TRUE);
        g_string_free(symbols->language_beginning_mark, TRUE);
        g_string_free(symbols->language_end_mark, TRUE);
        g_string_free(symbols->snippet_appending_mark, TRUE);
        g_string_free(symbols->snippet_prepending_mark, TRUE);
        g_string_free(symbols->tag_beginning_mark, TRUE);
        g_string_free(symbols->tag_end_mark, TRUE);
        g_string_free(symbols->snippet_reference_beginning_mark, TRUE);
        g_string_free(symbols->snippet_reference_end_mark, TRUE);
        free(symbols);
        g_option_context_free(option_context);
        return 0;
}
