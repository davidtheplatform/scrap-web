#include "raylib.h"
#include "vec.h"

#include <stdio.h>
#include <stddef.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define BLOCK_TEXT_SIZE (conf.font_size * 0.6)
#define DATA_PATH "data/"

struct Config {
    int font_size;
    int side_bar_size;
    char *font_symbols;
};

typedef enum {
    INPUT_TEXT_DISPLAY,
    INPUT_STRING,
    INPUT_IMAGE_DISPLAY,
} BlockInputType;

typedef union {
    char* text;
    Texture2D image;
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

// registered_blocks should be initialized before calling this
int block_register(char* id, Color color) {
    Blockdef* block = vector_add_dst(&registered_blocks);
    block->id = id;
    block->color = color;
    block->inputs = vector_create();

    return vector_size(registered_blocks) - 1;
}

void block_add_text(int block_id, char* text) {
    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_TEXT_DISPLAY;
    input->data = (BlockInputData) {
        .text = text,
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
    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_IMAGE_DISPLAY;
    input->data = (BlockInputData) {
        .image = texture,
    };
}

void block_unregister(int block_id) {
    vector_free(registered_blocks[block_id].inputs);
    vector_remove(registered_blocks, block_id);
}

bool draw_block(Vector2 position, int id) {
    bool collision = false;
    Vector2 mouse_pos = GetMousePosition();
    Blockdef block = registered_blocks[id];
    Color color = block.color;

    Rectangle final_size = (Rectangle) {
        position.x, position.y,
        5, conf.font_size,
    };

    collision = collision || CheckCollisionPointRec(mouse_pos, final_size);
    DrawRectangleRec(final_size, color );
    position.x += 5;

    for (int i = 0; i < vector_size(block.inputs); i++) {
        int width = 0;
        BlockInput cur = block.inputs[i];

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = MeasureTextEx(font_cond, cur.data.text, BLOCK_TEXT_SIZE, 0.0).x + 5;
            break;
        case INPUT_STRING:
            width = conf.font_size - 8 + 5;
            break;
        case INPUT_IMAGE_DISPLAY:
            width = (float)(conf.font_size - 8) / (float)cur.data.image.height * (float)cur.data.image.width + 5;
            break;
        default:
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x + 5;
            break;
        }

        collision = collision || CheckCollisionPointRec(mouse_pos, (Rectangle) { position.x, position.y, width, conf.font_size });
        DrawRectangle(position.x, position.y, width, conf.font_size, color);

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            DrawTextEx(font_cond, cur.data.text, (Vector2) { position.x, position.y + conf.font_size * 0.5 - BLOCK_TEXT_SIZE * 0.5, }, BLOCK_TEXT_SIZE, 0.0, WHITE);
            break;
        case INPUT_STRING:
            DrawRectangle(position.x, position.y + 4, width - 5, conf.font_size - 8, WHITE);
            break;
        case INPUT_IMAGE_DISPLAY:
            DrawTextureEx(cur.data.image, (Vector2) { position.x, position.y + 4 }, 0.0, (float)(conf.font_size - 8) / (float)cur.data.image.height, WHITE);
            break;
        default:
            DrawTextEx(font_cond, "NODEF", (Vector2) { position.x, position.y + conf.font_size * 0.5 - BLOCK_TEXT_SIZE * 0.5, }, BLOCK_TEXT_SIZE, 0.0, RED);
            break;
        }

        final_size.width += width;
        position.x += width;
    }

    DrawRectangleLinesEx(final_size, 2.0, ColorBrightness(color, collision ? 0.5 : -0.2));

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

    if (selected || CheckCollisionPointRec(GetMousePosition(), rect)) {
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

void draw_top_buttons(int sw, int sh) {
    Vector2 pos = (Vector2){ 0.0, conf.font_size };
    for (int i = 0; i < ARRLEN(top_buttons_text); i++) {
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

void setup(void) {
    run_tex = LoadTexture(DATA_PATH "run.png");
    SetTextureFilter(run_tex, TEXTURE_FILTER_BILINEAR);

    int codepoints_count;
    int *codepoints = LoadCodepoints(conf.font_symbols, &codepoints_count);
    font = LoadFontEx(DATA_PATH "nk57.otf", conf.font_size, codepoints, codepoints_count);
    font_cond = LoadFontEx(DATA_PATH "nk57-cond.otf", conf.font_size, codepoints, codepoints_count);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_cond.texture, TEXTURE_FILTER_BILINEAR);

    registered_blocks = vector_create();

    int set_xy = block_register("set_xy", (Color) { 0x00, 0x77, 0xff, 0xFF });
    block_add_text(set_xy, "Set X:");
    block_add_string_input(set_xy);
    block_add_text(set_xy, "Y:");
    block_add_string_input(set_xy);

    int move_steps = block_register("move_forward", (Color) { 0x00, 0x77, 0xff, 0xFF });
    block_add_text(move_steps, "Move");
    block_add_string_input(move_steps);
    block_add_text(move_steps, "pixels");

    int on_start = block_register("on_start", (Color) { 0xff, 0x77, 0x00, 0xFF });
    block_add_text(on_start, "When");
    block_add_image(on_start, run_tex);
    block_add_text(on_start, "clicked");
}

void free_registered_blocks(void) {
    for (int i = 0; i < vector_size(registered_blocks); i++) {
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
        BeginDrawing();
        ClearBackground(GetColor(0x202020ff));

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        DrawRectangle(0, 0, sw, conf.font_size, (Color){ 0x30, 0x30, 0x30, 0xFF });
        DrawRectangle(0, conf.font_size, sw, conf.font_size, (Color){ 0x2B, 0x2B, 0x2B, 0xFF });

        draw_top_buttons(sw, sh);

        BeginScissorMode(0, conf.font_size * 2, conf.side_bar_size, sh - conf.font_size * 2);
            DrawRectangle(0, conf.font_size * 2, conf.side_bar_size, sh - conf.font_size * 2, (Color){ 0, 0, 0, 0x40 });

            int pos_y = conf.font_size * 2 + 10;
            for (int i = 0; i < vector_size(registered_blocks); i++) {
                draw_block((Vector2){ 10, pos_y }, i);
                pos_y += conf.font_size + 10;
            }
        EndScissorMode();

        DrawTextEx(font, "Scrap", (Vector2){ 5, conf.font_size * 0.1 }, conf.font_size * 0.8, 0.0, WHITE);

        EndDrawing();
    }

    free_registered_blocks();
    CloseWindow();

    return 0;
}
