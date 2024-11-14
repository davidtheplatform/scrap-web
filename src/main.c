// TODO:
// - Input access
// - Better collision resolution
// - Block insertion

#include "raylib.h"
#include "vec.h"

#include <stdio.h>
#include <stddef.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define BLOCK_TEXT_SIZE (conf.font_size * 0.6)
#define DATA_PATH "data/"

struct Config {
    int font_size;
    int side_bar_size;
    char *font_symbols;
};

typedef struct Measurement {
    Vector2 size;
} Measurement;

typedef struct {
    Measurement ms;
    char* text;
} InputStaticText;

typedef struct {
    Measurement ms;
    Texture2D image;
} InputStaticImage;

typedef enum {
    INPUT_TEXT_DISPLAY,
    INPUT_STRING,
    INPUT_IMAGE_DISPLAY,
} BlockInputType;

typedef union {
    InputStaticText stext;
    char* text;
    InputStaticImage simage;
} BlockInputData;

typedef struct {
    BlockInputType type;
    BlockInputData data;
} BlockInput;

typedef struct {
    char* id;
    Color color;
    BlockInput* inputs;
} Blockdef;

typedef struct Block {
    int id;
    Vector2 pos;
    Measurement ms;
    struct Block* next;
    struct Block* prev;
} Block;

typedef struct {
    bool sidebar;
    Block* block;
} HoverInfo;

char *top_buttons_text[] = {
    "Code",
    "Game",
    "Sprites",
    "Sounds",
};

struct Config conf;
Texture2D run_tex;
Font font;
Font font_cond;

Blockdef* registered_blocks;
Block mouse_block = {0};
Block* sidebar;
Block* sprite_code;
HoverInfo hover_info = {0};

void update_root_nodes(void) {
    for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
        if (!sprite_code[i].next) continue;
        sprite_code[i].next->prev = &sprite_code[i];
    }
}

void update_measurements(Block* block) {
    if (block->id == -1) return;
    Blockdef blockdef = registered_blocks[block->id];

    block->ms.size.x = 5;
    block->ms.size.y = conf.font_size;

    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        Measurement ms;
        switch (blockdef.inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            ms = blockdef.inputs[i].data.stext.ms;
            break;
        case INPUT_STRING:
            ms.size.x = conf.font_size - 8;
            ms.size.y = conf.font_size - 8;
            break;
        case INPUT_IMAGE_DISPLAY:
            ms = blockdef.inputs[i].data.simage.ms;
            break;
        default:
            ms.size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            break;
        }
        ms.size.x += 5;

        block->ms.size.x += ms.size.x;
        block->ms.size.y = MAX(block->ms.size.y, ms.size.y + 8);
    }
}

Block new_block(int id) {
    Block block;
    block.id = id;
    block.pos = (Vector2) {0};
    block.ms = (Measurement) {0};
    block.next = NULL;
    block.prev = NULL;

    update_measurements(&block);
    return block;
}

void free_block(Block* block) {
    Block* cur = block->next;

    while (cur) {
        Block* next = cur->next;
        printf("free\n");
        free(cur);
        cur = next;
    }
}

// registered_blocks should be initialized before calling this
int block_register(char* id, Color color) {
    Blockdef* block = vector_add_dst(&registered_blocks);
    block->id = id;
    block->color = color;
    block->inputs = vector_create();

    return vector_size(registered_blocks) - 1;
}

void block_add_text(int block_id, char* text) {
    Measurement ms = {0};
    ms.size = MeasureTextEx(font_cond, text, BLOCK_TEXT_SIZE, 0.0);

    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_TEXT_DISPLAY;
    input->data = (BlockInputData) {
        .stext = {
            .text = text,
            .ms = ms,
        },
    };
}

void block_add_string_input(int block_id) {
    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_STRING;
    input->data = (BlockInputData) {
        .text = "",
    };
}

void block_add_image(int block_id, Texture2D texture) {
    Measurement ms = {0};
    ms.size.x = (float)(conf.font_size - 8) / (float)texture.height * (float)texture.width;
    ms.size.y = conf.font_size - 8;

    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_IMAGE_DISPLAY;
    input->data = (BlockInputData) {
        .simage = {
            .image = texture,
            .ms = ms,
        }
    };
}

void block_unregister(int block_id) {
    vector_free(registered_blocks[block_id].inputs);
    vector_remove(registered_blocks, block_id);
}

bool draw_block(Vector2 position, Block* block) {
    if (block->id == -1) return false;

    bool collision = false;
    Vector2 mouse_pos = GetMousePosition();
    Blockdef blockdef = registered_blocks[block->id];
    Color color = blockdef.color;

    Vector2 cursor = position;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = block->ms.size.x;
    block_size.height = block->ms.size.y;

    collision = collision || CheckCollisionPointRec(mouse_pos, block_size);
    DrawRectangleRec(block_size, ColorBrightness(color, collision ? 0.5 : 0.0));
    DrawRectangleLinesEx(block_size, 2.0, ColorBrightness(color, collision ? 0.5 : -0.2));
    cursor.x += 5;

    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        int width = 0;
        BlockInput cur = blockdef.inputs[i];

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x + 5;
            DrawTextEx(
                font_cond, 
                cur.data.stext.text, 
                (Vector2) { 
                    cursor.x, 
                    cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                },
                BLOCK_TEXT_SIZE,
                0.0,
                WHITE
            );
            break;
        case INPUT_STRING:
            width = conf.font_size - 8 + 5;
            DrawRectangle(cursor.x, cursor.y + 4, width - 5, conf.font_size - 8, WHITE);
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur.data.simage.ms.size.x + 5;
            DrawTextureEx(
                cur.data.simage.image, 
                (Vector2) { 
                    cursor.x, 
                    cursor.y + 4,
                }, 
                0.0, 
                (float)(conf.font_size - 8) / (float)cur.data.simage.image.height, 
                WHITE
            );
            break;
        default:
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x + 5;
            DrawTextEx(
                font_cond, 
                "NODEF", 
                (Vector2) { 
                    cursor.x, 
                    cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                },
                BLOCK_TEXT_SIZE, 
                0.0, 
                RED
            );
            break;
        }

        cursor.x += width;
    }

#ifdef DEBUG
    DrawTextEx(font_cond, TextFormat("Prev: %p", block->prev), (Vector2) { cursor.x + 10, cursor.y }, conf.font_size * 0.5, 0.0, WHITE);
    DrawTextEx(font_cond, TextFormat("%p", block), (Vector2) { cursor.x + 10, cursor.y + conf.font_size - conf.font_size * 0.5 }, conf.font_size * 0.5, 0.0, WHITE);
#endif

    return collision;
}

Vector2 draw_button(Vector2 position, char* text, float text_scale, int padding, int margin, bool selected) {
    int text_size = conf.font_size * text_scale;
    int text_width = text ? MeasureTextEx(font_cond, text, text_size, 0.0).x : 0;
    Rectangle rect = {
        .x = position.x,
        .y = position.y,
        .width = text_width + padding * 2,
        .height = conf.font_size,
    };

    if (selected || (CheckCollisionPointRec(GetMousePosition(), rect) && mouse_block.id == -1)) {
        Color select_color = selected ? (Color){ 0xDD, 0xDD, 0xDD, 0xDD } :
                                        (Color){ 0x40, 0x40, 0x40, 0xFF };
        DrawRectangleRec(rect, select_color);
    }
    if (text) {
        Color text_select_color = selected ? (Color){ 0x00, 0x00, 0x00, 0xFF } :
                                             (Color){ 0xCC, 0xCC, 0xCC, 0xFF };
        DrawTextEx(font_cond, text, (Vector2){ rect.x + padding, rect.y + conf.font_size * 0.5 - text_size * 0.5 }, text_size, 0.0, text_select_color);
    }

    return (Vector2){ rect.x + rect.width + margin, rect.y };
}

void draw_top_buttons(int sw) {
    Vector2 pos = (Vector2){ 0.0, conf.font_size };
    for (vec_size_t i = 0; i < ARRLEN(top_buttons_text); i++) {
        pos = draw_button(pos, top_buttons_text[i], 0.6, 10, 0, i == 0);
    }

    Vector2 run_pos = (Vector2){ sw - conf.font_size, conf.font_size };
    draw_button(run_pos, NULL, 0, conf.font_size * 0.5, 0, false);
    DrawTextureEx(run_tex, run_pos, 0, (float)conf.font_size / (float)run_tex.width, WHITE);
}

void set_default_config(void) {
    conf.font_size = 32;
    conf.side_bar_size = 300;
    conf.font_symbols = "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNMйцукенгшщзхъфывапролджэячсмитьбюёЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ,./;'\\[]=-0987654321`~!@#$%^&*()_+{}:\"|<>?";
}

void handle_mouse_click() {
    if (hover_info.sidebar) {
        if (mouse_block.id == -1 && hover_info.block) {
            mouse_block = *hover_info.block;
        } else {
            free_block(&mouse_block);
            mouse_block = new_block(-1);
        }
        return;
    }

    if (mouse_block.id != -1) {
        mouse_block.pos = GetMousePosition();
        if (hover_info.block && hover_info.block->next == NULL) {
            printf("malloc\n");
            Block* next = malloc(sizeof(mouse_block));
            *next = mouse_block;
            next->prev = hover_info.block;
            hover_info.block->next = next;
            if (next->next) {
                next->next->prev = next;
            }
        } else {
            vector_add(&sprite_code, mouse_block);
            update_root_nodes();
        }
        mouse_block = new_block(-1);
    } else if (hover_info.block) {
        if (hover_info.block->prev) {
            hover_info.block->prev->next = NULL;
            hover_info.block->prev = NULL;
            if (!IsKeyDown(KEY_BACKSPACE)) {
                mouse_block = *hover_info.block;
            } else {
                free_block(hover_info.block);
            }
            printf("free\n");
            free(hover_info.block);
        } else {
            if (!IsKeyDown(KEY_BACKSPACE)) {
                mouse_block = *hover_info.block;
            } else {
                free_block(hover_info.block);
            }
            vector_remove(sprite_code, hover_info.block - sprite_code); // Evil pointer arithmetic >:)
            update_root_nodes();
        }
    }
}

void setup(void) {
    run_tex = LoadTexture(DATA_PATH "run.png");
    SetTextureFilter(run_tex, TEXTURE_FILTER_BILINEAR);

    int codepoints_count;
    int *codepoints = LoadCodepoints(conf.font_symbols, &codepoints_count);
    font = LoadFontEx(DATA_PATH "nk57.otf", conf.font_size, codepoints, codepoints_count);
    font_cond = LoadFontEx(DATA_PATH "nk57-cond.otf", conf.font_size, codepoints, codepoints_count);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_cond.texture, TEXTURE_FILTER_BILINEAR);

    mouse_block = new_block(-1);
    registered_blocks = vector_create();

    int on_start = block_register("on_start", (Color) { 0xff, 0x77, 0x00, 0xFF });
    block_add_text(on_start, "When");
    block_add_image(on_start, run_tex);
    block_add_text(on_start, "clicked");

    int move_steps = block_register("move_steps", (Color) { 0x00, 0x77, 0xff, 0xFF });
    block_add_text(move_steps, "Move");
    block_add_string_input(move_steps);
    block_add_text(move_steps, "steps");

    int sc_print = block_register("print", (Color) { 0x00, 0xaa, 0x44, 0xFF });
    block_add_text(sc_print, "Print");
    block_add_string_input(sc_print);

    sidebar = vector_create();
    sprite_code = vector_create();
    for (vec_size_t i = 0; i < vector_size(registered_blocks); i++) {
        vector_add(&sidebar, new_block(i));
        printf("Size X: %.3f, Y: %.3f\n", sidebar[i].ms.size.x, sidebar[i].ms.size.y);
    }
}

void free_registered_blocks(void) {
    for (vec_size_t i = 0; i < vector_size(registered_blocks); i++) {
        block_unregister(i);
    }
    vector_free(registered_blocks);
}

int main(void) {
    set_default_config();

    InitWindow(800, 600, "Scrap");
    SetTargetFPS(60);
    EnableEventWaiting();
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    setup();

    while (!WindowShouldClose()) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            handle_mouse_click();
        }

        hover_info.sidebar = GetMouseX() < conf.side_bar_size && GetMouseY() > conf.font_size * 2;
        hover_info.block = NULL;

        BeginDrawing();
        ClearBackground(GetColor(0x202020ff));

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        DrawRectangle(0, 0, sw, conf.font_size, (Color){ 0x30, 0x30, 0x30, 0xFF });
        DrawRectangle(0, conf.font_size, sw, conf.font_size, (Color){ 0x2B, 0x2B, 0x2B, 0xFF });

        draw_top_buttons(sw);

        BeginScissorMode(0, conf.font_size * 2, conf.side_bar_size, sh - conf.font_size * 2);
            DrawRectangle(0, conf.font_size * 2, conf.side_bar_size, sh - conf.font_size * 2, (Color){ 0, 0, 0, 0x40 });

            int pos_y = conf.font_size * 2 + 10;
            for (vec_size_t i = 0; i < vector_size(sidebar); i++) {
                if (draw_block((Vector2){ 10, pos_y }, &sidebar[i]) && !hover_info.block) {
                    hover_info.block = &sidebar[i];
                };
                pos_y += conf.font_size + 10;
            }

        EndScissorMode();

        BeginScissorMode(conf.side_bar_size, conf.font_size * 2, sw - conf.side_bar_size, sh - conf.font_size * 2);
            for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
                Block* cur = &sprite_code[i];
                Vector2 pos = sprite_code[i].pos;
                do {
                    if (draw_block(pos, cur) && !hover_info.block) {
                        hover_info.block = cur;
                    };
                    cur = cur->next;
                    pos.y += conf.font_size;
                } while (cur);
            }
        EndScissorMode();

        BeginScissorMode(0, conf.font_size * 2, sw, sh - conf.font_size * 2);
            Block* cur = &mouse_block;
            Vector2 pos = GetMousePosition();
            do {
                draw_block(pos, cur);
                cur = cur->next;
                pos.y += conf.font_size;
            } while (cur);
#ifdef DEBUG
            DrawTextEx(
                font_cond, 
                TextFormat("Block: %p\nSidebar: %d", hover_info.block, hover_info.sidebar), 
                (Vector2){ 
                    conf.side_bar_size + 5, 
                    conf.font_size * 2 + 5
                }, 
                conf.font_size * 0.5,
                0.0, 
                GRAY
            );
#endif
        EndScissorMode();

        DrawTextEx(font, "Scrap", (Vector2){ 5, conf.font_size * 0.1 }, conf.font_size * 0.8, 0.0, WHITE);

        EndDrawing();
    }

    free_registered_blocks();
    CloseWindow();

    return 0;
}
