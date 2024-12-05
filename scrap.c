// TODO:
// - Better collision resolution
// - Swap blocks inside arguments?
// - Codebase scrollbars

#include "raylib.h"
#include "vec.h"

#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MOD(x, y) (((x) % (y) + (y)) % (y))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))
#define LERP(min, max, t) (((max) - (min)) * (t) + (min))
#define UNLERP(min, max, v) (((float)(v) - (float)(min)) / ((float)(max) - (float)(min)))

#define BLOCK_TEXT_SIZE (conf.font_size * 0.6)
#define DATA_PATH "data/"
#define BLOCK_PADDING (5.0 * (float)conf.font_size / 32.0)
#define BLOCK_OUTLINE_SIZE (2.0 * (float)conf.font_size / 32.0)
#define BLOCK_STRING_PADDING (10.0 * (float)conf.font_size / 32.0)
#define BLOCK_CONTROL_INDENT (16.0 * (float)conf.font_size / 32.0)
#define DROP_TEX_WIDTH ((float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height * (float)drop_tex.width)
#define ACTION_BAR_MAX_SIZE 128

struct Config {
    int font_size;
    int side_bar_size;
    char *font_symbols;
};

typedef struct Measurement {
    Vector2 size;
} Measurement;

typedef enum {
    BLOCKCONSTR_UNLIMITED, // Can put anything as argument
    BLOCKCONSTR_STRING, // Can only put strings as argument
} BlockArgumentConstraint;

typedef enum {
    DROPDOWN_SOURCE_LISTREF,
} BlockDropdownSource;

typedef struct {
    Measurement ms;
    char* text;
} InputStaticText;

typedef struct {
    BlockArgumentConstraint constr;
    Measurement ms;
    char* text;
} InputArgument;

typedef struct {
    Measurement ms;
    Texture2D image;
} InputStaticImage;

typedef enum {
    INPUT_TEXT_DISPLAY,
    INPUT_ARGUMENT,
    INPUT_DROPDOWN,
    INPUT_IMAGE_DISPLAY,
} BlockInputType;

typedef enum {
    BLOCKTYPE_NORMAL,
    BLOCKTYPE_CONTROL,
    BLOCKTYPE_CONTROLEND,
    BLOCKTYPE_END,
} BlockType;

typedef struct Block {
    int id;
    void* arguments; // arguments are of void type because of C. That makes the whole codebase a bit garbled, though :P
    Measurement ms;
    struct Block* parent;
} Block;

typedef char** (*ListAccessor)(Block* block, size_t* list_len);

typedef struct {
    BlockDropdownSource source;
    ListAccessor list;
} InputDropdown;

typedef union {
    InputStaticText stext;
    InputStaticImage simage;
    InputArgument arg;
    InputDropdown drop;
} BlockInputData;

typedef struct {
    BlockInputType type;
    BlockInputData data;
} BlockInput;

typedef struct {
    char* id;
    Color color;
    BlockType type;
    bool hidden;
    BlockInput* inputs;
} Blockdef;

typedef enum {
    ARGUMENT_TEXT,
    ARGUMENT_BLOCK,
    ARGUMENT_CONST_STRING,
} BlockArgumentType;

typedef union {
    char* text;
    Block block;
} BlockArgumentData;

typedef struct {
    Measurement ms;
    vec_size_t input_id;
    BlockArgumentType type;
    BlockArgumentData data;
} BlockArgument;

typedef struct {
    Vector2 pos;
    Block* block;
} DrawStack;

typedef struct {
    Vector2 pos;
    DrawStack* draw_stack;
    Block* blocks;
} BlockChain;

typedef enum {
    TOPBAR_TOP,
    TOPBAR_TABS,
    TOPBAR_RUN_BUTTON,
} TopBarType;

typedef struct {
    TopBarType type;
    int ind;
} TopBars;

typedef struct {
    bool sidebar;
    BlockChain* blockchain;
    vec_size_t blockchain_index;
    int blockchain_layer;
    Block* block;
    BlockArgument* argument;
    Vector2 argument_pos;
    BlockArgument* prev_argument;
    Block* select_block;
    BlockArgument* select_argument;
    Vector2 select_argument_pos;
    Vector2 last_mouse_pos;
    Vector2 mouse_click_pos;
    float time_at_last_pos;
    int dropdown_hover_ind;
    bool drag_cancelled;
    TopBars top_bars;
} HoverInfo;

typedef struct {
    Measurement ms;
    int scroll_amount;
} Dropdown;

typedef struct {
    float show_time;
    char text[ACTION_BAR_MAX_SIZE];
} ActionBar;

typedef struct {
    int min;
    int max;
    int value;
    char* label;
} IGuiSlider;

typedef enum {
    ELEMENT_SLIDER,
    ELEMENT_CLOSE_BUTTON,
} IGuiElementType;

typedef union {
    IGuiSlider slider;
} IGuiElementData;

typedef struct {
    IGuiElementType type;
    IGuiElementData data;
} IGuiElement;

typedef enum {
    IGUI_SETTINGS,
} IGuiType;

typedef struct {
    bool shown;
    float animation_time;
    bool is_fading;
    Vector2 size;
    int hover_element;
    int select_element;
    IGuiType type;
    IGuiElement* elements;
} IGui;

char* top_bar_buttons_text[] = {
    "File",
    "Settings",
    "About",
};

char* tab_bar_buttons_text[] = {
    "Code",
    "Output",
};

struct Config conf;
Texture2D run_tex;
Texture2D drop_tex;
Texture2D close_tex;
#ifdef LOGO
Texture2D logo_tex;
#endif
Font font;
Font font_cond;
Font font_eb;
Shader line_shader;
float shader_time = 0.0;
int shader_time_loc;

Blockdef* registered_blocks;
Block* sidebar; // Vector that contains block prototypes
BlockChain mouse_blockchain = {0};
BlockChain* sprite_code; // List of block chains
HoverInfo hover_info = {0};
Dropdown dropdown = {0};
ActionBar actionbar;
IGui igui = {0};
int end_block_id = -1;

Vector2 camera_pos = {0};
Vector2 camera_click_pos = {0};
int blockchain_select_counter = -1;

char* keys_list[] = {
    "Space", "Enter",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", 
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", 
    "U", "V", "W", "X", "Y", "Z",
};

const char* line_shader_vertex =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec4 vertexColor;\n"
    "out vec2 fragCoord;\n"
    "out vec4 fragColor;\n"
    "uniform mat4 mvp;\n"
    "void main() {\n"
    "    vec4 pos = mvp * vec4(vertexPosition, 1.0);\n"
    "    fragCoord = pos.xy;\n"
    "    fragColor = vertexColor;\n"
    "    gl_Position = pos;\n"
    "}";

const char* line_shader_fragment =
    "#version 330\n"
    "in vec2 fragCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform float time = 0.0;\n"
    "void main() {\n"
    "    vec2 coord = (fragCoord + 1.0) * 0.5;\n"
    "    coord.y = 1.0 - coord.y;\n"
    "    float pos = time * 4.0 - 1.0;\n"
    "    float diff = clamp(1.0 - abs(coord.x + coord.y - pos), 0.0, 1.0);\n"
    "    finalColor = vec4(fragColor.xyz, pow(diff, 2.0));\n"
    "}";

char** keys_accessor(Block* block, size_t* list_len) {
    (void)block;
    *list_len = ARRLEN(keys_list);
    return keys_list;
}

void update_measurements(Block* block);
void load_fonts(bool reload);

void actionbar_show(const char* text) {
    strncpy(actionbar.text, text, ACTION_BAR_MAX_SIZE);
    actionbar.show_time = 3.0;
}

void block_update_parent_links(Block* block) {
    BlockArgument* block_args = block->arguments;
    for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block_args[i].type != ARGUMENT_BLOCK) continue;
        block_args[i].data.block.parent = block;
    }
}

Block block_new(int id) {
    Block block;
    block.id = id;
    block.ms = (Measurement) {0};
    block.arguments = id == -1 ? NULL : vector_create();
    block.parent = NULL;

    if (id != -1) {
        printf("New block\n\tId: %d\n", id);
        Blockdef blockdef = registered_blocks[block.id];
        for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
            if (blockdef.inputs[i].type != INPUT_ARGUMENT && blockdef.inputs[i].type != INPUT_DROPDOWN) continue;
            BlockArgument* arg = vector_add_dst((BlockArgument**)&block.arguments);
            arg->data.text = vector_create();
            arg->input_id = i;

            switch (blockdef.inputs[i].type) {
            case INPUT_ARGUMENT:
                arg->ms = blockdef.inputs[i].data.arg.ms;
                switch (blockdef.inputs[i].data.arg.constr) {
                case BLOCKCONSTR_UNLIMITED:
                    arg->type = ARGUMENT_TEXT;
                    break;
                case BLOCKCONSTR_STRING:
                    arg->type = ARGUMENT_CONST_STRING;
                    break;
                default:
                    assert(false && "Unimplemented argument constraint");
                    break;
                }

                for (char* pos = blockdef.inputs[i].data.arg.text; *pos; pos++) {
                    vector_add(&arg->data.text, *pos);
                }
                break;
            case INPUT_DROPDOWN:
                arg->type = ARGUMENT_CONST_STRING;
                arg->ms = (Measurement) {0};

                size_t list_len = 0;
                char** list = blockdef.inputs[i].data.drop.list(&block, &list_len);
                if (!list || list_len == 0) break;

                for (char* pos = list[0]; *pos; pos++) {
                    vector_add(&arg->data.text, *pos);
                }
                break;
            default:
                assert(false && "Unreachable");
                break;
            }
            vector_add(&arg->data.text, 0);
        }
    }

    update_measurements(&block);
    return block;
}

// Broken at the moment, not sure why
Block block_copy(Block* block) {
    printf("Copy block id: %d\n", block->id);
    if (!block->arguments) return *block;

    Block new = *block;
    new.arguments = vector_create();
    for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
        BlockArgument* block_args = block->arguments;
        BlockArgument* arg = vector_add_dst((BlockArgument**)&new.arguments);
        arg->ms = block_args[i].ms;
        arg->type = block_args[i].type;
        switch (block_args[i].type) {
        case ARGUMENT_TEXT:
            arg->data.text = vector_copy(block_args[i].data.text);
            break;
        case ARGUMENT_BLOCK:
            arg->data.block = block_copy(&block_args[i].data.block);
            break;
        default:
            assert(false && "Unimplemented argument copy");
            break;
        }
    }

    return new;
}

void block_free(Block* block) {
    printf("Free block id: %d\n", block->id);

    if (block->arguments) {
        for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
            BlockArgument* block_args = block->arguments;
            switch (block_args[i].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                vector_free(block_args[i].data.text);
                break;
            case ARGUMENT_BLOCK:
                block_free(&block_args[i].data.block);
                break;
            default:
                assert(false && "Unimplemented argument free");
                break;
            }
        }
        vector_free((BlockArgument*)block->arguments);
    }
}

BlockChain blockchain_new() {
    printf("New blockchain\n");
    BlockChain chain;
    chain.pos = (Vector2) {0};
    chain.blocks = vector_create();
    chain.draw_stack = vector_create();

    return chain;
}

void blockchain_update_parent_links(BlockChain* chain) {
    for (vec_size_t i = 0; i < vector_size(chain->blocks); i++) {
        block_update_parent_links(&chain->blocks[i]);
    }
}

void blockchain_add_block(BlockChain* chain, Block block) {
    vector_add(&chain->blocks, block);
    blockchain_update_parent_links(chain);
}

void blockchain_clear_blocks(BlockChain* chain) {
    for (vec_size_t i = 0; i < vector_size(chain->blocks); i++) {
        block_free(&chain->blocks[i]);
    }
    vector_clear(chain->blocks);
}

void blockchain_insert(BlockChain* dst, BlockChain* src, vec_size_t pos) {
    assert(pos < vector_size(dst->blocks));

    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks));
    for (ssize_t i = (ssize_t)vector_size(src->blocks) - 1; i >= 0; i--) {
        vector_insert(&dst->blocks, pos + 1, src->blocks[i]);
    }
    blockchain_update_parent_links(dst);
    vector_clear(src->blocks);
}

void blockchain_detach(BlockChain* dst, BlockChain* src, vec_size_t pos) {
    assert(pos < vector_size(src->blocks));

    int current_layer = hover_info.blockchain_layer;
    int layer_size = 0;

    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks) - pos);
    for (vec_size_t i = pos; i < vector_size(src->blocks); i++) {
        BlockType block_type = registered_blocks[src->blocks[i].id].type;
        if ((block_type == BLOCKTYPE_END || (block_type == BLOCKTYPE_CONTROLEND && i != pos)) && hover_info.blockchain_layer == current_layer && current_layer != 0) break;
        vector_add(&dst->blocks, src->blocks[i]);
        if (block_type == BLOCKTYPE_CONTROL) {
            current_layer++;
        } else if (block_type == BLOCKTYPE_END) {
            current_layer--;
        }
        layer_size++;
    }
    blockchain_update_parent_links(dst);
    vector_erase(src->blocks, pos, layer_size);
    blockchain_update_parent_links(src);
}

void blockchain_free(BlockChain* chain) {
    printf("Free blockchain\n");
    blockchain_clear_blocks(chain);
    vector_free(chain->blocks);
    vector_free(chain->draw_stack);
}

void argument_set_block(BlockArgument* block_arg, Block block) {
    assert(block_arg->type == ARGUMENT_TEXT);

    vector_free(block_arg->data.text);
    block_arg->type = ARGUMENT_BLOCK;
    block_arg->data.block = block;

    block_update_parent_links(&block_arg->data.block);
    update_measurements(&block_arg->data.block);
}

void argument_set_const_string(BlockArgument* block_arg, Block* block, char* text) {
    assert(block_arg->type == ARGUMENT_CONST_STRING);

    block_arg->type = ARGUMENT_CONST_STRING;
    vector_clear(block_arg->data.text);

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);

    Measurement ms = {0};
    ms.size = MeasureTextEx(font_cond, text, BLOCK_TEXT_SIZE, 0.0);
    block_arg->ms = ms;

    update_measurements(block);
}

void argument_set_text(BlockArgument* block_arg, char* text) {
    assert(block_arg->type == ARGUMENT_BLOCK);
    assert(block_arg->data.block.parent != NULL);

    Block* parent = block_arg->data.block.parent;

    block_arg->type = ARGUMENT_TEXT;
    block_arg->data.text = vector_create();

    for (char* pos = text; *pos; pos++) {
        vector_add(&block_arg->data.text, *pos);
    }
    vector_add(&block_arg->data.text, 0);

    Measurement ms = {0};
    ms.size = MeasureTextEx(font_cond, text, BLOCK_TEXT_SIZE, 0.0);
    block_arg->ms = ms;

    update_measurements(parent);
}

// registered_blocks should be initialized before calling this
int block_register(char* id, BlockType type, Color color) {
    Blockdef* blockdef = vector_add_dst(&registered_blocks);
    blockdef->id = id;
    blockdef->color = color;
    blockdef->type = type;
    blockdef->hidden = false;
    blockdef->inputs = vector_create();

    if (type == BLOCKTYPE_END && end_block_id == -1) {
        end_block_id = vector_size(registered_blocks) - 1;
        blockdef->hidden = true;
    }

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

void block_add_argument(int block_id, char* defualt_data, BlockArgumentConstraint constraint) {
    Measurement ms = {0};
    ms.size = MeasureTextEx(font_cond, defualt_data, BLOCK_TEXT_SIZE, 0.0);
    ms.size.x += BLOCK_STRING_PADDING;
    ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.x);

    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_ARGUMENT;
    input->data = (BlockInputData) {
        .arg = {
            .text = defualt_data,
            .constr = constraint,
            .ms = ms,
        },
    };
}

void block_add_dropdown(int block_id, BlockDropdownSource dropdown_source, ListAccessor accessor) {
    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_DROPDOWN;
    input->data = (BlockInputData) {
        .drop = {
            .source = dropdown_source,
            .list = accessor,
        },
    };
}

void block_add_image(int block_id, Texture2D texture) {
    Measurement ms = {0};
    ms.size.x = (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)texture.height * (float)texture.width;
    ms.size.y = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

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

void update_measurements(Block* block) {
    if (block->id == -1) return;
    Blockdef blockdef = registered_blocks[block->id];

    block->ms.size.x = BLOCK_PADDING;
    block->ms.size.y = conf.font_size;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        Measurement ms;
        BlockArgument* block_args = block->arguments;

        switch (blockdef.inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            ms = blockdef.inputs[i].data.stext.ms;
            break;
        case INPUT_ARGUMENT:
            switch (block_args[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                Measurement string_ms;
                string_ms.size = MeasureTextEx(font_cond, block_args[arg_id].data.text, BLOCK_TEXT_SIZE, 0.0);
                string_ms.size.x += BLOCK_STRING_PADDING;
                string_ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, string_ms.size.x);

                block_args[arg_id].ms = string_ms;
                ms = string_ms;
                ms.size.y = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.y);
                break;
            case ARGUMENT_BLOCK:
                block_args[arg_id].ms = block_args[arg_id].data.block.ms;
                ms = block_args[arg_id].ms;
                break;
            default:
                assert(false && "Unimplemented argument measure");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            switch (block_args[arg_id].type) {
            case ARGUMENT_CONST_STRING:
                Measurement string_ms;
                string_ms.size = MeasureTextEx(font_cond, block_args[arg_id].data.text, BLOCK_TEXT_SIZE, 0.0);
                string_ms.size.x += BLOCK_STRING_PADDING + DROP_TEX_WIDTH;
                string_ms.size.x = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, string_ms.size.x);

                block_args[arg_id].ms = string_ms;
                ms = string_ms;
                ms.size.y = MAX(conf.font_size - BLOCK_OUTLINE_SIZE * 4, ms.size.y);
                break;
            case ARGUMENT_TEXT:
            case ARGUMENT_BLOCK:
                assert(false && "Illegal argument type");
                break;
            default:
                assert(false && "Unimplemented argument measure");
                break;
            }
            arg_id++;
            break;
        case INPUT_IMAGE_DISPLAY:
            ms = blockdef.inputs[i].data.simage.ms;
            break;
        default:
            ms.size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            break;
        }
        ms.size.x += BLOCK_PADDING;

        block->ms.size.x += ms.size.x;
        block->ms.size.y = MAX(block->ms.size.y, ms.size.y + BLOCK_OUTLINE_SIZE * 4);
    }

    if (block->parent) update_measurements(block->parent);
}

void block_update_collisions(Vector2 position, Block* block) {
    if (hover_info.block && !block->parent) return;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = block->ms.size.x;
    block_size.height = block->ms.size.y;
    
    if (!CheckCollisionPointRec(GetMousePosition(), block_size)) return;
    hover_info.block = block;

    Vector2 cursor = position;
    cursor.x += BLOCK_PADDING;

    Blockdef blockdef = registered_blocks[block->id];
    int arg_id = 0;

    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        if (hover_info.argument) return;
        int width = 0;
        BlockInput cur = blockdef.inputs[i];
        BlockArgument* block_args = block->arguments;
        Rectangle arg_size;

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            break;
        case INPUT_ARGUMENT:
            width = block_args[arg_id].ms.size.x;

            switch (block_args[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = block_args[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block_args[arg_id];
                    hover_info.argument_pos = cursor;
                    break;
                }
                break;
            case ARGUMENT_BLOCK:
                Vector2 block_pos;
                block_pos.x = cursor.x;
                block_pos.y = cursor.y + block_size.height / 2 - block_args[arg_id].ms.size.y / 2; 

                arg_size.x = block_pos.x;
                arg_size.y = block_pos.y;
                arg_size.width = block_args[arg_id].ms.size.x;
                arg_size.height = block_args[arg_id].ms.size.y;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.prev_argument = &block_args[arg_id];
                }
                
                block_update_collisions(block_pos, &block_args[arg_id].data.block);
                break;
            default:
                assert(false && "Unimplemented argument collision");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            width = block_args[arg_id].ms.size.x;

            switch (block_args[arg_id].type) {
            case ARGUMENT_CONST_STRING:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = block_args[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block_args[arg_id];
                    hover_info.argument_pos = cursor;
                    break;
                }
                break;
            case ARGUMENT_TEXT:
                assert(false && "Illegal argument type ARGUMENT_TEXT");
                break;
            case ARGUMENT_BLOCK:
                assert(false && "Illegal argument type ARGUMENT_BLOCK");
                break;
            default:
                assert(false && "Unimplemented argument collision");
                break;
            }
            arg_id++;
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur.data.simage.ms.size.x;
            break;
        default:
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x;
            break;
        }
        cursor.x += width + BLOCK_PADDING;
    }
}

void draw_text_shadow(Font font, const char *text, Vector2 position, float font_size, float spacing, Color tint, Color shadow) {
    DrawTextEx(font, text, (Vector2) { position.x + 1, position.y + 1 }, font_size, spacing, shadow);
    DrawTextEx(font, text, position, font_size, spacing, tint);
}

void draw_block(Vector2 position, Block* block, bool force_outline) {
    if (block->id == -1) return;

    bool collision = hover_info.block == block;
    Blockdef blockdef = registered_blocks[block->id];
    Color color = blockdef.color;

    Vector2 cursor = position;

    Rectangle block_size;
    block_size.x = position.x;
    block_size.y = position.y;
    block_size.width = block->ms.size.x;
    block_size.height = block->ms.size.y;

    DrawRectangleRec(block_size, ColorBrightness(color, collision ? 0.3 : 0.0));
    if (force_outline || (blockdef.type != BLOCKTYPE_CONTROL && blockdef.type != BLOCKTYPE_CONTROLEND)) {
        DrawRectangleLinesEx(block_size, BLOCK_OUTLINE_SIZE, ColorBrightness(color, collision ? 0.5 : -0.2));
    }
    cursor.x += BLOCK_PADDING;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        int width = 0;
        BlockInput cur = blockdef.inputs[i];
        BlockArgument* block_args = block->arguments;

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            draw_text_shadow(
                font_cond, 
                cur.data.stext.text, 
                (Vector2) { 
                    cursor.x, 
                    cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                },
                BLOCK_TEXT_SIZE,
                0.0,
                WHITE,
                (Color) { 0x00, 0x00, 0x00, 0x88 }
            );
            break;
        case INPUT_ARGUMENT:
            width = block_args[arg_id].ms.size.x;

            switch (block_args[arg_id].type) {
            case ARGUMENT_CONST_STRING:
            case ARGUMENT_TEXT:
                Rectangle arg_size;
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = width;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                bool hovered = &block_args[arg_id] == hover_info.argument;
                bool selected = &block_args[arg_id] == hover_info.select_argument;

                if (block_args[arg_id].type == ARGUMENT_CONST_STRING) {
                    DrawRectangleRounded(arg_size, 0.5, 5, WHITE);
                    if (hovered || selected) {
                        DrawRectangleRoundedLines(arg_size, 0.5, 5, BLOCK_OUTLINE_SIZE, ColorBrightness(color, selected ? -0.5 : 0.5));
                    }
                } else if (block_args[arg_id].type == ARGUMENT_TEXT) {
                    DrawRectangleRec(arg_size, WHITE);
                    if (hovered || selected) {
                        DrawRectangleLinesEx(arg_size, BLOCK_OUTLINE_SIZE, ColorBrightness(color, selected ? -0.5 : 0.2));
                    }
                } 
                DrawTextEx(
                    font_cond, 
                    block_args[arg_id].data.text,
                    (Vector2) { 
                        cursor.x + BLOCK_STRING_PADDING * 0.5, 
                        cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                    },
                    BLOCK_TEXT_SIZE,
                    0.0,
                    BLACK
                );
                break;
            case ARGUMENT_BLOCK:
                Vector2 block_pos;
                block_pos.x = cursor.x;
                block_pos.y = cursor.y + block_size.height * 0.5 - block_args[arg_id].ms.size.y * 0.5;

                draw_block(block_pos, &block_args[arg_id].data.block, true);
                break;
            default:
                assert(false && "Unimplemented argument draw");
                break;
            }
            arg_id++;
            break;
        case INPUT_DROPDOWN:
            width = block_args[arg_id].ms.size.x;

            switch (block_args[arg_id].type) {
            case ARGUMENT_CONST_STRING:
                Rectangle arg_size;
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_OUTLINE_SIZE * 4) * 0.5;
                arg_size.width = width;
                arg_size.height = conf.font_size - BLOCK_OUTLINE_SIZE * 4;

                DrawRectangleRounded(arg_size, 0.5, 4, ColorBrightness(color, collision ? 0.0 : -0.3));

                if (&block_args[arg_id] == hover_info.argument || &block_args[arg_id] == hover_info.select_argument) {
                    DrawRectangleRoundedLines(arg_size, 0.5, 4, BLOCK_OUTLINE_SIZE, ColorBrightness(color, &block_args[arg_id] == hover_info.select_argument ? -0.5 : 0.5));
                }
                Vector2 ms = MeasureTextEx(font_cond, block_args[arg_id].data.text, BLOCK_TEXT_SIZE, 0);
                draw_text_shadow(
                    font_cond, 
                    block_args[arg_id].data.text,
                    (Vector2) { 
                        cursor.x + BLOCK_STRING_PADDING * 0.5, 
                        cursor.y + block_size.height * 0.5 - BLOCK_TEXT_SIZE * 0.5, 
                    },
                    BLOCK_TEXT_SIZE,
                    0.0,
                    WHITE,
                    (Color) { 0x00, 0x00, 0x00, 0x88 }
                );
                DrawTextureEx(
                    drop_tex,
                    (Vector2) { 
                        cursor.x + ms.x + BLOCK_STRING_PADDING * 0.5 + 1,
                        cursor.y + BLOCK_OUTLINE_SIZE * 2 + 1,
                    }, 
                    0.0, 
                    (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height, 
                    (Color) { 0x00, 0x00, 0x00, 0x88 }
                );

                DrawTextureEx(
                    drop_tex,
                    (Vector2) { 
                        cursor.x + ms.x + BLOCK_STRING_PADDING * 0.5,
                        cursor.y + BLOCK_OUTLINE_SIZE * 2,
                    }, 
                    0.0, 
                    (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)drop_tex.height, 
                    WHITE
                );

                break;
            case ARGUMENT_TEXT:
                assert(false && "Illegal argument type ARGUMENT_TEXT");
                break;
            case ARGUMENT_BLOCK:
                assert(false && "Illegal argument type ARGUMENT_BLOCK");
                break;
            default:
                assert(false && "Unimplemented argument draw");
                break;
            }
            arg_id++;
            break;
        case INPUT_IMAGE_DISPLAY:
            width = cur.data.simage.ms.size.x;
            DrawTextureEx(
                cur.data.simage.image, 
                (Vector2) { 
                    cursor.x + 1, 
                    cursor.y + BLOCK_OUTLINE_SIZE * 2 + 1,
                }, 
                0.0, 
                (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)cur.data.simage.image.height, 
                (Color) { 0x00, 0x00, 0x00, 0x88 }
            );
            DrawTextureEx(
                cur.data.simage.image, 
                (Vector2) { 
                    cursor.x, 
                    cursor.y + BLOCK_OUTLINE_SIZE * 2,
                }, 
                0.0, 
                (float)(conf.font_size - BLOCK_OUTLINE_SIZE * 4) / (float)cur.data.simage.image.height, 
                WHITE
            );
            break;
        default:
            width = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0).x;
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

        cursor.x += width + BLOCK_PADDING;
    }
}

// Draw order for draw_control_outline() and draw_controlend_outline()
//         1    12
//   +-----|---------+ 
//   |               | 2
//   |     +---------+
//   | 10  |    3
// 4 |     | 8
//   |-----|    7
//   |  9  +---------+
//   | 11            |
//   |               | 6
//   +---------------+
//         5

void draw_controlend_outline(DrawStack* block, Vector2 end_pos, Color color) {
    BlockType blocktype = registered_blocks[block->block->id].type;
    Vector2 block_size = block->block->ms.size;
    
    if (blocktype == BLOCKTYPE_CONTROL) {
        /* 1 */ DrawRectangle(block->pos.x, block->pos.y, block_size.x, BLOCK_OUTLINE_SIZE, color);
    } else if (blocktype == BLOCKTYPE_CONTROLEND) {
        /* 12 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    }
    /* 2 */ DrawRectangle(block->pos.x + block_size.x - BLOCK_OUTLINE_SIZE, block->pos.y, BLOCK_OUTLINE_SIZE, block_size.y, color);
    /* 3 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y - BLOCK_OUTLINE_SIZE, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    /* 8 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y, BLOCK_OUTLINE_SIZE, end_pos.y - (block->pos.y + block_size.y), color);
    /* 10 */ DrawRectangle(block->pos.x, block->pos.y, BLOCK_OUTLINE_SIZE, end_pos.y - block->pos.y, color);
}

void draw_control_outline(DrawStack* block, Vector2 end_pos, Color color, bool draw_end) {
    BlockType blocktype = registered_blocks[block->block->id].type;
    Vector2 block_size = block->block->ms.size;

    if (blocktype == BLOCKTYPE_CONTROL) {
        /* 1 */ DrawRectangle(block->pos.x, block->pos.y, block_size.x, BLOCK_OUTLINE_SIZE, color);
    } else if (blocktype == BLOCKTYPE_CONTROLEND) {
        /* 12 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    }
    /* 2 */ DrawRectangle(block->pos.x + block_size.x - BLOCK_OUTLINE_SIZE, block->pos.y, BLOCK_OUTLINE_SIZE, block_size.y, color);
    /* 3 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y - BLOCK_OUTLINE_SIZE, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    if (draw_end) {
        /* 4 */ DrawRectangle(block->pos.x, block->pos.y, BLOCK_OUTLINE_SIZE, end_pos.y + conf.font_size - block->pos.y, color);
        /* 5 */ DrawRectangle(end_pos.x, end_pos.y + conf.font_size - BLOCK_OUTLINE_SIZE, block_size.x, BLOCK_OUTLINE_SIZE, color);
        /* 6 */ DrawRectangle(end_pos.x + block_size.x - BLOCK_OUTLINE_SIZE, end_pos.y, BLOCK_OUTLINE_SIZE, conf.font_size, color);
        /* 7 */ DrawRectangle(end_pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, end_pos.y, block_size.x - BLOCK_CONTROL_INDENT + BLOCK_OUTLINE_SIZE, BLOCK_OUTLINE_SIZE, color);
    } else {
        /* 9 */ DrawRectangle(end_pos.x, end_pos.y - BLOCK_OUTLINE_SIZE, BLOCK_CONTROL_INDENT, BLOCK_OUTLINE_SIZE, color);
        /* 10 */ DrawRectangle(block->pos.x, block->pos.y, BLOCK_OUTLINE_SIZE, end_pos.y - block->pos.y, color);
    }
    /* 8 */ DrawRectangle(block->pos.x + BLOCK_CONTROL_INDENT - BLOCK_OUTLINE_SIZE, block->pos.y + block_size.y, BLOCK_OUTLINE_SIZE, end_pos.y - (block->pos.y + block_size.y), color);
}

void blockchain_check_collisions(BlockChain* chain, Vector2 camera_pos) {
    vector_clear(chain->draw_stack);

    hover_info.blockchain = chain;
    hover_info.blockchain_layer = 0;
    Vector2 pos = hover_info.blockchain->pos;
    pos.x -= camera_pos.x;
    pos.y -= camera_pos.y;
    for (vec_size_t i = 0; i < vector_size(hover_info.blockchain->blocks); i++) {
        if (hover_info.block) break;
        hover_info.blockchain_layer = vector_size(chain->draw_stack);
        hover_info.blockchain_index = i;

        Blockdef blockdef = registered_blocks[chain->blocks[i].id];
        if ((blockdef.type == BLOCKTYPE_END || blockdef.type == BLOCKTYPE_CONTROLEND) && vector_size(chain->draw_stack) > 0) {
            pos.x -= BLOCK_CONTROL_INDENT;

            if (blockdef.type == BLOCKTYPE_END) {
                DrawStack prev_block = chain->draw_stack[vector_size(chain->draw_stack) - 1];
                Rectangle rect;
                rect.x = pos.x;
                rect.y = pos.y;
                rect.width = prev_block.block->ms.size.x;
                rect.height = conf.font_size;

                if (CheckCollisionPointRec(GetMousePosition(), rect)) {
                    hover_info.block = &hover_info.blockchain->blocks[i];
                }
            } else if (blockdef.type == BLOCKTYPE_CONTROLEND) {
                block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
            }
            vector_pop(chain->draw_stack);
        } else {
            block_update_collisions(pos, &hover_info.blockchain->blocks[i]);
        }

        if (blockdef.type == BLOCKTYPE_CONTROL || blockdef.type == BLOCKTYPE_CONTROLEND) {
            DrawStack stack_item;
            stack_item.pos = pos;
            stack_item.block = &chain->blocks[i];
            vector_add(&chain->draw_stack, stack_item);
            pos.x += BLOCK_CONTROL_INDENT;
        }
        pos.y += hover_info.blockchain->blocks[i].ms.size.y;
    }
}

void draw_block_chain(BlockChain* chain, Vector2 camera_pos) {
    vector_clear(chain->draw_stack);

    Vector2 pos = chain->pos;
    pos.x -= camera_pos.x;
    pos.y -= camera_pos.y;
    for (vec_size_t i = 0; i < vector_size(chain->blocks); i++) {
        Blockdef blockdef = registered_blocks[chain->blocks[i].id];
        if ((blockdef.type == BLOCKTYPE_END || blockdef.type == BLOCKTYPE_CONTROLEND) && vector_size(chain->draw_stack) > 0) {
            pos.x -= BLOCK_CONTROL_INDENT;
            DrawStack prev_block = chain->draw_stack[vector_size(chain->draw_stack) - 1];
            Blockdef prev_blockdef = registered_blocks[prev_block.block->id];

            Rectangle rect;
            rect.x = prev_block.pos.x;
            rect.y = prev_block.pos.y + prev_block.block->ms.size.y;
            rect.width = BLOCK_CONTROL_INDENT;
            rect.height = pos.y - (prev_block.pos.y + prev_block.block->ms.size.y);
            DrawRectangleRec(rect, prev_blockdef.color);

            if (blockdef.type == BLOCKTYPE_END) {
                DrawRectangle(pos.x, pos.y, prev_block.block->ms.size.x, conf.font_size, ColorBrightness(prev_blockdef.color, hover_info.block == &chain->blocks[i] ? 0.3 : 0.0));
                draw_control_outline(
                    &prev_block, 
                    pos, 
                    ColorBrightness(prev_blockdef.color, hover_info.block == prev_block.block || hover_info.block == &chain->blocks[i] ? 0.5 : -0.2),
                    true
                );
            } else if (blockdef.type == BLOCKTYPE_CONTROLEND) {
                draw_block(pos, &chain->blocks[i], false);
                draw_controlend_outline(
                    &prev_block, 
                    pos, 
                    ColorBrightness(prev_blockdef.color, hover_info.block == prev_block.block || hover_info.block == &chain->blocks[i] ? 0.5 : -0.2)
                );
            }

            vector_pop(chain->draw_stack);
        } else {
            draw_block(pos, &chain->blocks[i], false);
        }
        if (blockdef.type == BLOCKTYPE_CONTROL || blockdef.type == BLOCKTYPE_CONTROLEND) {
            DrawStack stack_item;
            stack_item.pos = pos;
            stack_item.block = &chain->blocks[i];
            vector_add(&chain->draw_stack, stack_item);
            pos.x += BLOCK_CONTROL_INDENT;
        }
        pos.y += chain->blocks[i].ms.size.y;
    }

    pos.y += conf.font_size;
    Rectangle rect;
    for (vec_size_t i = 0; i < vector_size(chain->draw_stack); i++) {
        DrawStack prev_block = chain->draw_stack[i];
        Blockdef prev_blockdef = registered_blocks[prev_block.block->id];

        pos.x = prev_block.pos.x;

        rect.x = prev_block.pos.x;
        rect.y = prev_block.pos.y + prev_block.block->ms.size.y;
        rect.width = BLOCK_CONTROL_INDENT;
        rect.height = pos.y - (prev_block.pos.y + prev_block.block->ms.size.y);

        DrawRectangleRec(rect, prev_blockdef.color);
        draw_control_outline(&prev_block, pos, ColorBrightness(prev_blockdef.color, hover_info.block == prev_block.block ? 0.5 : -0.2), false);
    }
}

bool button_check_collisions(Vector2* position, char* text, float button_scale, float side_padding, float side_margin) {
    side_padding *= conf.font_size;
    side_margin *= conf.font_size;

    int text_size = conf.font_size * 0.6;
    int text_width = text ? MeasureTextEx(font_cond, text, text_size, 0.0).x : 0;
    Rectangle rect = {
        .x = position->x,
        .y = position->y,
        .width = text_width + side_padding * 2,
        .height = conf.font_size * button_scale,
    };

    position->x += rect.width + side_margin;
    return CheckCollisionPointRec(GetMousePosition(), rect) && vector_size(mouse_blockchain.blocks) == 0;
}

void draw_button(Vector2* position, char* text, float button_scale, float side_padding, float side_margin, bool selected, bool hovered) {
    side_padding *= conf.font_size;
    side_margin *= conf.font_size;

    int text_size = conf.font_size * 0.6;
    int text_width = text ? MeasureTextEx(font_cond, text, text_size, 0.0).x : 0;
    Rectangle rect = {
        .x = position->x,
        .y = position->y,
        .width = text_width + side_padding * 2,
        .height = conf.font_size * button_scale,
    };

    if (selected || hovered) {
        Color select_color = selected ? (Color){ 0xFF, 0xFF, 0xFF, 0xFF } :
                                        (Color){ 0x40, 0x40, 0x40, 0xFF };
        DrawRectangleRec(rect, select_color);
    }
    if (text) {
        Color text_select_color = selected ? (Color){ 0x00, 0x00, 0x00, 0xFF } :
                                             (Color){ 0xFF, 0xFF, 0xFF, 0xFF };
        DrawTextEx(font_cond, text, (Vector2){ rect.x + side_padding, rect.y + rect.height * 0.5 - text_size * 0.5 }, text_size, 0.0, text_select_color);
    }

    position->x += rect.width + side_margin;
}

void bars_check_collisions(void) {
    Vector2 pos = (Vector2){ 0.0, conf.font_size * 1.2 };
    for (vec_size_t i = 0; i < ARRLEN(tab_bar_buttons_text); i++) {
        if (button_check_collisions(&pos, tab_bar_buttons_text[i], 1.0, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TABS;
            hover_info.top_bars.ind = i;
            return;
        }
    }

    Vector2 run_pos = (Vector2){ GetScreenWidth() - conf.font_size, conf.font_size * 1.2 };
    if (button_check_collisions(&run_pos, NULL, 1.0, 0.5, 0)) {
        hover_info.top_bars.type = TOPBAR_RUN_BUTTON;
        hover_info.top_bars.ind = 0;
        return;
    }

    int width = MeasureTextEx(font_eb, "Scrap", conf.font_size * 0.8, 0.0).x;
    pos = (Vector2){ 20 + conf.font_size + width, 0 };
    for (vec_size_t i = 0; i < ARRLEN(top_bar_buttons_text); i++) {
        if (button_check_collisions(&pos, top_bar_buttons_text[i], 1.2, 0.3, 0)) {
            hover_info.top_bars.type = TOPBAR_TOP;
            hover_info.top_bars.ind = i;
            return;
        }
    }
}

#define COLLISION_AT(bar_type, index) (hover_info.top_bars.type == (bar_type) && hover_info.top_bars.ind == (int)(index))
void draw_tab_buttons(int sw) {
    Vector2 pos = (Vector2){ 0.0, conf.font_size * 1.2 };
    for (vec_size_t i = 0; i < ARRLEN(tab_bar_buttons_text); i++) {
        draw_button(&pos, tab_bar_buttons_text[i], 1.0, 0.3, 0, i == 0, COLLISION_AT(TOPBAR_TABS, i));
    }

    Vector2 run_pos = (Vector2){ sw - conf.font_size, conf.font_size * 1.2 };
    Vector2 run_pos_copy = run_pos;
    draw_button(&run_pos_copy, NULL, 1.0, 0.5, 0, false, COLLISION_AT(TOPBAR_RUN_BUTTON, 0));
    DrawTextureEx(run_tex, run_pos, 0, (float)conf.font_size / (float)run_tex.width, WHITE);
}

void draw_top_bar(void) {
#ifdef LOGO
    DrawTexture(logo_tex, 5, conf.font_size * 0.1, WHITE);
#endif

    int width = MeasureTextEx(font_eb, "Scrap", conf.font_size * 0.8, 0.0).x;
    DrawTextEx(font_eb, "Scrap", (Vector2){ 10 + conf.font_size, conf.font_size * 0.2 }, conf.font_size * 0.8, 0.0, WHITE);

    Vector2 pos = { 20 + conf.font_size + width, 0 };

    for (vec_size_t i = 0; i < ARRLEN(top_bar_buttons_text); i++) {
        draw_button(&pos, top_bar_buttons_text[i], 1.2, 0.3, 0, false, COLLISION_AT(TOPBAR_TOP, i));
    }
}
#undef COLLISION_AT

void draw_tooltip(void) {
    if (hover_info.time_at_last_pos < 0.5 || !hover_info.block) return;

    Vector2 pos = GetMousePosition();
    pos.x += 10.0;
    pos.y += 10.0;

    char* text = "Amog";
    Vector2 ms = MeasureTextEx(font_cond, text, conf.font_size * 0.5, 0);   
    DrawRectangle(pos.x - 5, pos.y - 5, ms.x + 10, ms.y + 10, (Color) { 0x00, 0x00, 0x00, 0x80 });
    DrawTextEx(font_cond, text, pos, conf.font_size * 0.5, 0, WHITE);
}

void draw_dropdown_list(void) {
    if (!hover_info.select_argument) return;

    Blockdef blockdef = registered_blocks[hover_info.select_block->id];
    BlockInput block_input = blockdef.inputs[hover_info.select_argument->input_id];

    if (block_input.type != INPUT_DROPDOWN) return;
    
    Vector2 pos;
    pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;

    DrawRectangle(pos.x, pos.y, dropdown.ms.size.x, dropdown.ms.size.y, ColorBrightness(blockdef.color, -0.3));
    if (hover_info.dropdown_hover_ind != -1) {
        DrawRectangle(pos.x, pos.y + (hover_info.dropdown_hover_ind - dropdown.scroll_amount) * conf.font_size, dropdown.ms.size.x, conf.font_size, blockdef.color);
    }

    pos.x += 5.0;
    pos.y += 5.0;

    size_t list_len = 0;
    char** list = block_input.data.drop.list(hover_info.select_block, &list_len);
    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        draw_text_shadow(font_cond, list[i], pos, BLOCK_TEXT_SIZE, 0, WHITE, (Color) { 0x00, 0x00, 0x00, 0x88 });
        pos.y += conf.font_size;
    }
}

void draw_dots(void) {
    int win_width = GetScreenWidth();
    int win_height = GetScreenHeight();

    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
            DrawPixel(x, y, (Color) { 0x60, 0x60, 0x60, 0xff });
        }
    }

    BeginShaderMode(line_shader);
    for (int y = MOD(-(int)camera_pos.y, conf.font_size * 2); y < win_height; y += conf.font_size * 2) {
        DrawLine(0, y, win_width, y, (Color) { 0x40, 0x40, 0x40, 0xff });
    }
    for (int x = MOD(-(int)camera_pos.x, conf.font_size * 2); x < win_width; x += conf.font_size * 2) {
        DrawLine(x, 0, x, win_height, (Color) { 0x40, 0x40, 0x40, 0xff });
    }
    EndShaderMode();
}

void draw_action_bar(void) {
    if (actionbar.show_time <= 0) return;

    int width = MeasureTextEx(font_eb, actionbar.text, conf.font_size * 0.75, 0.0).x;
    Vector2 pos;
    pos.x = (GetScreenWidth() - conf.side_bar_size) / 2 - width / 2 + conf.side_bar_size;
    pos.y = GetScreenHeight() * 0.15;
    Color color = YELLOW;
    color.a = actionbar.show_time / 3.0 * 255.0;

    DrawTextEx(font_eb, actionbar.text, pos, conf.font_size * 0.75, 0.0, color);
}

// https://easings.net/#easeOutExpo
float ease_out_expo(float x) {
    return x == 1.0 ? 1.0 : 1 - powf(2.0, -10.0 * x);
}

void igui_draw_window(void) {
    if (!igui.shown) return;

    float animation_ease = ease_out_expo(igui.animation_time);

    Vector2 size = igui.size;
    size.x *= animation_ease * GetScreenWidth();
    size.y *= animation_ease * GetScreenHeight();

    Rectangle rect;
    rect.x = GetScreenWidth() / 2 - size.x / 2;
    rect.y = GetScreenHeight() / 2 - size.y / 2;
    rect.width = size.x;
    rect.height = size.y;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 0x00, 0x00, 0x00, 0x60 * animation_ease });
    DrawRectangleRec(rect, (Color) { 0x30, 0x30, 0x30, 0xff });
    DrawRectangleLinesEx(rect, 5.0, (Color) { 0x20, 0x20, 0x20, 0xff });
    BeginShaderMode(line_shader);
    DrawRectangleLinesEx(rect, 5.0, (Color) { 0x80, 0x80, 0x80, 0xff });
    EndShaderMode();

    int width = MeasureTextEx(font_eb, "Settings", conf.font_size * animation_ease, 0.0).x;
    DrawTextEx(font_eb, "Settings", (Vector2) { rect.x + rect.width / 2 - width / 2, rect.y + 10 }, conf.font_size * animation_ease, 0.0, WHITE);

    Vector2 pos;
    pos.x = rect.x + 10;
    pos.y = rect.y + conf.font_size * animation_ease + 20;

    for (vec_size_t i = 0; i < vector_size(igui.elements); i++) {
        bool hovered = igui.hover_element == (int)i;
        bool selected = igui.select_element == (int)i;

        switch (igui.elements[i].type) {
        case ELEMENT_SLIDER:
            IGuiSlider slider = igui.elements[i].data.slider;

            int label_width = slider.label ? MeasureTextEx(font_cond, slider.label, conf.font_size * 0.8 * animation_ease, 0.0).x : 0;
            draw_text_shadow(font_cond, slider.label, pos, conf.font_size * 0.8 * animation_ease, 0.0, WHITE, (Color) { 0x00, 0x00, 0x00, 0x88 });

            Rectangle el_size;
            el_size.x = pos.x + label_width + 10;
            el_size.y = pos.y;
            el_size.width = rect.width - 30 - label_width;
            el_size.height = conf.font_size * animation_ease;
            DrawRectangleRec(el_size, (Color) { 0x45, 0x45, 0x45, 0xff });
            DrawRectangleLinesEx(el_size, 2.5 * animation_ease, ColorBrightness((Color) { 0x40, 0x40, 0x40, 0xff }, hovered || selected ? 0.5 : 0.0));

            float knob_size = conf.font_size * animation_ease * 0.75;
            Rectangle knob;
            knob.x = LERP(el_size.x + 5, el_size.x + el_size.width - knob_size - 5, UNLERP(slider.min, slider.max, slider.value));
            knob.y = el_size.y + el_size.height / 2 - knob_size / 2;
            knob.width = knob_size;
            knob.height = knob_size;

            DrawRectangleRec(knob, (Color) { 0x80, 0x80, 0x80, 0xff });
            DrawRectangleLinesEx(knob, 2.5 * animation_ease, hovered || selected ? WHITE : (Color) { 0x70, 0x70, 0x70, 0xff});
            BeginShaderMode(line_shader);
            DrawRectangleLinesEx(knob, 2.5 * animation_ease, WHITE);
            EndShaderMode();
            
            if (selected) {
                const char* select_hint = TextFormat("%d", slider.value);
                int select_hint_width = MeasureTextEx(font_eb, select_hint, conf.font_size * 0.5, 0.0).x;
                draw_text_shadow(
                    font_eb, 
                    select_hint, 
                    (Vector2) { knob.x + knob.width / 2 - select_hint_width / 2, knob.y - conf.font_size * 0.5 - 10 }, 
                    conf.font_size * 0.5, 
                    0.0, 
                    YELLOW,
                    (Color) { 0x00, 0x00, 0x00, 0x88 }
                );
            }

            DrawTextEx(
                font_cond, 
                TextFormat(
                    "Lerp: %.3f, Value: %d", 
                    UNLERP(slider.min, slider.max, slider.value), slider.value
                ), 
                (Vector2) { knob.x + knob.width, knob.y + knob.height }, 
                conf.font_size * 0.5 * animation_ease, 
                0.0, 
                GRAY
            );
            break;
        case ELEMENT_CLOSE_BUTTON:
            float button_size = conf.font_size * animation_ease;
            Rectangle button;
            button.x = rect.x + rect.width - button_size - 10;
            button.y = rect.y + 10;
            button.width = button_size;
            button.height = button_size;

            DrawRectangleRec(button, (Color) { 0x45, 0x45, 0x45, 0xff });
            DrawRectangleLinesEx(button, 2.5 * animation_ease, ColorBrightness((Color) { 0x40, 0x40, 0x40, 0xff }, hovered ? 0.5 : 0.0));
            BeginShaderMode(line_shader);
            DrawRectangleLinesEx(button, 2.5 * animation_ease, WHITE);
            EndShaderMode();

            DrawTextureEx(close_tex, (Vector2) { button.x, button.y }, 0.0, conf.font_size / 32.0 * animation_ease, WHITE);

            pos.y -= conf.font_size * animation_ease + 10;
            break;
        default:
            assert(false && "Unimplemented IGui element draw");
            break;
        }

        pos.y += conf.font_size * animation_ease + 10;
    }

#ifdef DEBUG
    DrawTextEx(
        font_cond, 
        TextFormat(
            "Animation time: %.3f\n"
            "Hover: %d, Select: %d",
            igui.animation_time,
            igui.hover_element, igui.select_element
        ), 
        (Vector2) { rect.x + 5, rect.y + 5 }, 
        conf.font_size * 0.5 * animation_ease, 
        0.0, 
        GRAY
    );
#endif
}

void igui_show(void) {
    igui.is_fading = false;
    shader_time = -0.2;
}

void igui_close(void) {
    igui.is_fading = true;
}

void igui_clear_elements(void) {
    vector_clear(igui.elements);
}

void igui_add_slider(int min, int max, int value, char* label) {
    IGuiElement* el = vector_add_dst(&igui.elements);
    el->type = ELEMENT_SLIDER;
    el->data.slider = (IGuiSlider) {
        .min = min,
        .max = max,
        .value = value,
        .label = label,
    };
}

void igui_add_close_button(void) {
    IGuiElement* el = vector_add_dst(&igui.elements);
    el->type = ELEMENT_CLOSE_BUTTON;
    el->data = (IGuiElementData){0};
}

void igui_set_type(IGuiType type) {
    igui.type = type;
}

void igui_tick(void) {
    igui.hover_element = -1;

    if (igui.is_fading) {
        igui.animation_time = MAX(igui.animation_time - GetFrameTime() * 2.0, 0.0);
        if (igui.animation_time == 0.0) igui.shown = false;
    } else {
        igui.shown = true;
        igui.animation_time = MIN(igui.animation_time + GetFrameTime() * 2.0, 1.0);
    }

    if (!igui.shown || igui.animation_time != 1.0) return;

    Rectangle win_size;
    win_size.x = GetScreenWidth() / 2 - igui.size.x * GetScreenWidth() / 2;
    win_size.y = GetScreenHeight() / 2 - igui.size.y * GetScreenHeight() / 2;
    win_size.width = igui.size.x * GetScreenWidth();
    win_size.height = igui.size.y * GetScreenHeight();

    Vector2 pos;
    pos.x = win_size.x + 10;
    pos.y = win_size.y + conf.font_size + 20;

    for (vec_size_t i = 0; i < vector_size(igui.elements); i++) {
        if (igui.hover_element != -1) break;
        switch (igui.elements[i].type) {
        case ELEMENT_SLIDER:
            IGuiSlider slider = igui.elements[i].data.slider;

            int label_width = slider.label ? MeasureTextEx(font_cond, slider.label, conf.font_size * 0.8, 0.0).x : 0;

            Rectangle el_coll;
            el_coll.x = pos.x + label_width + 10;
            el_coll.y = pos.y;
            el_coll.width = win_size.width - 30 - label_width;
            el_coll.height = conf.font_size;

            if (CheckCollisionPointRec(GetMousePosition(), el_coll)) {
                igui.hover_element = i;
                break;
            }

            break;
        case ELEMENT_CLOSE_BUTTON:
            float button_size = conf.font_size;
            Rectangle button;
            button.x = win_size.x + win_size.width - button_size - 10;
            button.y = win_size.y + 10;
            button.width = button_size;
            button.height = button_size;

            if (CheckCollisionPointRec(GetMousePosition(), button)) {
                igui.hover_element = i;
                break;
            }

            pos.y -= conf.font_size + 10;
            break;
        default:
            assert(false && "Unimplemented IGui element collision");
            break;
        }

        pos.y += conf.font_size + 10;
    }

    if (igui.select_element == -1) return;

    switch (igui.elements[igui.select_element].type) {
    case ELEMENT_SLIDER:
        IGuiSlider* slider = &igui.elements[igui.select_element].data.slider;


        int label_width = slider->label ? MeasureTextEx(font_cond, slider->label, conf.font_size * 0.8, 0.0).x : 0;
        draw_text_shadow(font_cond, slider->label, pos, conf.font_size * 0.8, 0.0, WHITE, (Color) { 0x00, 0x00, 0x00, 0x88 });

        Rectangle el_size;
        el_size.x = pos.x + label_width + 10;
        el_size.y = pos.y;
        el_size.width = win_size.width - 30 - label_width;
        el_size.height = conf.font_size;

        float knob_size = conf.font_size * 0.75;
        float t = CLAMP(UNLERP(el_size.x + 5 + knob_size / 2, el_size.x + el_size.width - knob_size / 2 - 5, GetMouseX()), 0.0, 1.0);
        slider->value = roundf(LERP(slider->min, slider->max, t));
        break;
    default:
        break;
    }
}

void handle_igui_close(void) {
    if (igui.type == IGUI_SETTINGS && igui.elements[1].data.slider.value != conf.font_size) {
        printf("Reload fonts\n");
        conf.font_size = igui.elements[1].data.slider.value;
        load_fonts(true);
    }
}

// Return value indicates if we should cancel dragging
bool handle_mouse_click(void) {
    hover_info.mouse_click_pos = GetMousePosition();
    camera_click_pos = camera_pos;

    if (igui.shown) {
        if (igui.hover_element != -1) {
            igui.select_element = igui.hover_element;
            if (igui.elements[igui.hover_element].type == ELEMENT_CLOSE_BUTTON) {
                handle_igui_close();
                igui_close();
                return true;
            }
        }
        return true;
    }

    if (hover_info.top_bars.ind != -1) {
        if (hover_info.top_bars.type == TOPBAR_TOP && hover_info.top_bars.ind == 1) {
            igui.size.x = 0.8;
            igui.size.y = 0.8;
            igui_clear_elements();
            igui_add_close_button();
            igui_add_slider(8, 64, conf.font_size, "UI size");
            igui_add_slider(-300, 300, 0, "Other");
            igui_set_type(IGUI_SETTINGS);
            igui_show();
        }
        return true;
    }

    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover_info.sidebar) {
        if (hover_info.select_argument) {
            hover_info.select_argument = NULL;
            hover_info.select_argument_pos.x = 0;
            hover_info.select_argument_pos.y = 0;
            dropdown.scroll_amount = 0;
            return true;
        }
        if (mouse_empty && hover_info.block) {
            // Pickup block
            blockchain_add_block(&mouse_blockchain, block_new(hover_info.block->id));
            if (registered_blocks[hover_info.block->id].type == BLOCKTYPE_CONTROL && end_block_id != -1) {
                blockchain_add_block(&mouse_blockchain, block_new(end_block_id));
            }
            return true;
        } else if (!mouse_empty) {
            // Drop block
            blockchain_clear_blocks(&mouse_blockchain);
            return true;
        }
        return true;
    }

    if (mouse_empty) {
        if (hover_info.dropdown_hover_ind != -1) {
            Blockdef blockdef = registered_blocks[hover_info.select_block->id];
            BlockInput block_input = blockdef.inputs[hover_info.select_argument->input_id];
            assert(block_input.type == INPUT_DROPDOWN);
            
            size_t list_len = 0;
            char** list = block_input.data.drop.list(hover_info.select_block, &list_len);
            assert((size_t)hover_info.dropdown_hover_ind < list_len);

            argument_set_const_string(hover_info.select_argument, hover_info.select_block, list[hover_info.dropdown_hover_ind]);
        }

        if (hover_info.block != hover_info.select_block) {
            hover_info.select_block = hover_info.block;
        }

        if (hover_info.argument != hover_info.select_argument) {
            hover_info.select_argument = hover_info.argument;
            hover_info.select_argument_pos = hover_info.argument_pos;
            dropdown.scroll_amount = 0;
            return true;
        }

        if (hover_info.select_argument) {
            return true;
        }
    }

    if (!mouse_empty) {
        mouse_blockchain.pos = GetMousePosition();
        if (hover_info.argument) {
            // Attach to argument
            printf("Attach to argument\n");
            if (vector_size(mouse_blockchain.blocks) > 1) return true;
            if (hover_info.argument->type != ARGUMENT_TEXT) return true;
            mouse_blockchain.blocks[0].parent = hover_info.block;
            argument_set_block(hover_info.argument, mouse_blockchain.blocks[0]);
            vector_clear(mouse_blockchain.blocks);
        } else if (
            hover_info.block && 
            hover_info.blockchain && 
            hover_info.block->parent == NULL
        ) {
            // Attach block
            printf("Attach block\n");
            blockchain_insert(hover_info.blockchain, &mouse_blockchain, hover_info.blockchain_index);
            // Update block link to make valgrind happy
            hover_info.block = &hover_info.blockchain->blocks[hover_info.blockchain_index];
        } else {
            // Put block
            printf("Put block\n");
            mouse_blockchain.pos.x += camera_pos.x;
            mouse_blockchain.pos.y += camera_pos.y;
            vector_add(&sprite_code, mouse_blockchain);
            mouse_blockchain = blockchain_new();
        }
        return true;
    } else if (hover_info.block) {
        if (hover_info.block->parent) {
            // Detach argument
            printf("Detach argument\n");
            assert(hover_info.prev_argument != NULL);

            blockchain_add_block(&mouse_blockchain, *hover_info.block);
            mouse_blockchain.blocks[0].parent = NULL;
            argument_set_text(hover_info.prev_argument, "");
        } else if (hover_info.blockchain) {
            // Detach block
            printf("Detach block\n");
            blockchain_detach(&mouse_blockchain, hover_info.blockchain, hover_info.blockchain_index);
            if (hover_info.blockchain_index == 0) {
                blockchain_free(hover_info.blockchain);
                vector_remove(sprite_code, hover_info.blockchain - sprite_code);
                hover_info.block = NULL;
            }
        }
        return true;
    }
    return false;
}

void handle_key_press(void) {
    if (!hover_info.select_argument) {
        if (IsKeyPressed(KEY_SPACE) && vector_size(sprite_code) > 0) {
            blockchain_select_counter++;
            if ((vec_size_t)blockchain_select_counter >= vector_size(sprite_code)) blockchain_select_counter = 0;

            camera_pos.x = sprite_code[blockchain_select_counter].pos.x - GetScreenWidth() / 2;
            camera_pos.y = sprite_code[blockchain_select_counter].pos.y - GetScreenHeight() / 2;
            actionbar_show(TextFormat("Jump to chain (%d/%d)", blockchain_select_counter + 1, vector_size(sprite_code)));
        }
        return;
    };
    assert(hover_info.select_argument->type == ARGUMENT_TEXT || hover_info.select_argument->type == ARGUMENT_CONST_STRING);
    if (registered_blocks[hover_info.select_block->id].inputs[hover_info.select_argument->input_id].type == INPUT_DROPDOWN) return;

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        char* arg_text = hover_info.select_argument->data.text;
        if (vector_size(arg_text) <= 1) return;

        int remove_pos = vector_size(arg_text) - 2;
        int remove_size = 1;

        while (((unsigned char)arg_text[remove_pos] >> 6) == 2) { // This checks if we are in the middle of UTF-8 char
            remove_pos--;
            remove_size++;
        }

        vector_erase(arg_text, remove_pos, remove_size);
        update_measurements(hover_info.select_block);
        return;
    }

    int char_val;
    while ((char_val = GetCharPressed())) {
        char** arg_text = &hover_info.select_argument->data.text;
        
        int utf_size = 0;
        const char* utf_char = CodepointToUTF8(char_val, &utf_size);
        for (int i = 0; i < utf_size; i++) {
            vector_insert(arg_text, vector_size(*arg_text) - 1, utf_char[i]);
        }
        update_measurements(hover_info.select_block);
    }
}

void handle_mouse_wheel(void) {
    int wheel = (int)GetMouseWheelMove();

    dropdown.scroll_amount = MAX(dropdown.scroll_amount - wheel, 0);
}

void handle_mouse_drag(void) {
    if (hover_info.drag_cancelled) return;

    Vector2 mouse_pos = GetMousePosition();

    camera_pos.x = camera_click_pos.x - (mouse_pos.x - hover_info.mouse_click_pos.x);
    camera_pos.y = camera_click_pos.y - (mouse_pos.y - hover_info.mouse_click_pos.y);
}

void dropdown_check_collisions(void) {
    if (!hover_info.select_argument) return;

    Blockdef blockdef = registered_blocks[hover_info.select_block->id];
    BlockInput block_input = blockdef.inputs[hover_info.select_argument->input_id];

    if (block_input.type != INPUT_DROPDOWN) return;

    dropdown.ms.size.x = hover_info.select_argument->ms.size.x;
    dropdown.ms.size.y = 5.0;

    size_t list_len = 0;
    char** list = block_input.data.drop.list(hover_info.select_block, &list_len);

    Vector2 pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;
    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        Vector2 text_ms = MeasureTextEx(font_cond, list[i], BLOCK_TEXT_SIZE, 0);
        dropdown.ms.size.x = MAX(text_ms.x + 10, dropdown.ms.size.x);
        dropdown.ms.size.y += conf.font_size;
        pos.y += conf.font_size;
    }

    pos = hover_info.select_argument_pos;
    pos.y += hover_info.select_block->ms.size.y;

    for (size_t i = dropdown.scroll_amount; i < list_len; i++) {
        if (pos.y > GetScreenHeight()) break;
        Rectangle rect;
        rect.x = pos.x;
        rect.y = pos.y;
        rect.width = dropdown.ms.size.x;
        rect.height = conf.font_size;

        if (CheckCollisionPointRec(GetMousePosition(), rect)) {
            hover_info.dropdown_hover_ind = i;
            break;
        }
        pos.y += conf.font_size;
    }
}

void check_block_collisions(void) {
    if (hover_info.sidebar) {
        int pos_y = conf.font_size * 2 + 10;
        for (vec_size_t i = 0; i < vector_size(sidebar); i++) {
            if (hover_info.block) break;
            block_update_collisions((Vector2){ 10, pos_y }, &sidebar[i]);
            pos_y += conf.font_size + 10;
        }
    } else {
        for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
            if (hover_info.block) break;
            blockchain_check_collisions(&sprite_code[i], camera_pos);
        }
    }
}

void sanitize_block(Block* block) {
    BlockArgument* block_args = block->arguments;
    for (vec_size_t i = 0; i < vector_size(block_args); i++) {
        if (block_args[i].type != ARGUMENT_BLOCK) continue;
        if (block_args[i].data.block.parent != block) {
            printf("ERROR: Block %p detached from parent %p! (Got %p)\n", &block_args[i].data.block, block, block_args[i].data.block.parent);
            assert(false);
            return;
        }
        sanitize_block(&block_args[i].data.block);
    }
}

void sanitize_links(void) {
    for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
        Block* blocks = sprite_code[i].blocks;
        for (vec_size_t j = 0; j < vector_size(blocks); j++) {
            sanitize_block(&blocks[j]);
        }
    }

    for (vec_size_t i = 0; i < vector_size(mouse_blockchain.blocks); i++) {
        sanitize_block(&mouse_blockchain.blocks[i]);
    }
}

void set_default_config(void) {
    conf.font_size = 32;
    conf.side_bar_size = 300;
    conf.font_symbols = "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNMйцукенгшщзхъфывапролджэячсмитьбюёЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ,./;'\\[]=-0987654321`~!@#$%^&*()_+{}:\"|<>?";
}

void load_fonts(bool reload) {
    if (reload) {
        UnloadTexture(logo_tex);
        UnloadFont(font);
        UnloadFont(font_cond);
        UnloadFont(font_eb);
    }

#ifdef LOGO
    Image logo = LoadImageSvg(DATA_PATH "logo.svg", conf.font_size, conf.font_size);
    logo_tex = LoadTextureFromImage(logo);
    SetTextureFilter(logo_tex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(logo);
#endif

    int codepoints_count;
    int *codepoints = LoadCodepoints(conf.font_symbols, &codepoints_count);
    font = LoadFontEx(DATA_PATH "nk57.otf", conf.font_size, codepoints, codepoints_count);
    font_cond = LoadFontEx(DATA_PATH "nk57-cond.otf", conf.font_size, codepoints, codepoints_count);
    font_eb = LoadFontEx(DATA_PATH "nk57-eb.otf", conf.font_size, codepoints, codepoints_count);
    UnloadCodepoints(codepoints);

    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_cond.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(font_eb.texture, TEXTURE_FILTER_BILINEAR);
}

void setup(void) {
    run_tex = LoadTexture(DATA_PATH "run.png");
    SetTextureFilter(run_tex, TEXTURE_FILTER_BILINEAR);
    drop_tex = LoadTexture(DATA_PATH "drop.png");
    SetTextureFilter(drop_tex, TEXTURE_FILTER_BILINEAR);
    close_tex = LoadTexture(DATA_PATH "close.png");
    SetTextureFilter(close_tex, TEXTURE_FILTER_BILINEAR);

    load_fonts(false);

    line_shader = LoadShaderFromMemory(line_shader_vertex, line_shader_fragment);
    shader_time_loc = GetShaderLocation(line_shader, "time");

    registered_blocks = vector_create();

    int on_start = block_register("on_start", BLOCKTYPE_NORMAL, (Color) { 0xff, 0x77, 0x00, 0xFF });
    block_add_text(on_start, "When");
    block_add_image(on_start, run_tex);
    block_add_text(on_start, "clicked");

    int on_key_press = block_register("on_key_press", BLOCKTYPE_NORMAL, (Color) { 0xff, 0x77, 0x00, 0xFF });
    block_add_text(on_key_press, "When");
    block_add_dropdown(on_key_press, DROPDOWN_SOURCE_LISTREF, &keys_accessor);
    block_add_text(on_key_press, "pressed");

    int sc_print = block_register("print", BLOCKTYPE_NORMAL, (Color) { 0x00, 0xaa, 0x44, 0xFF });
    block_add_text(sc_print, "Print");
    block_add_argument(sc_print, "Привет, мусороид!", BLOCKCONSTR_UNLIMITED);

    int sc_loop = block_register("loop", BLOCKTYPE_CONTROL, (Color) { 0xff, 0x99, 0x00, 0xff });
    block_add_text(sc_loop, "Loop");

    int sc_if = block_register("if", BLOCKTYPE_CONTROL, (Color) { 0xff, 0x99, 0x00, 0xff });
    block_add_text(sc_if, "If");
    block_add_argument(sc_if, "", BLOCKCONSTR_UNLIMITED);
    block_add_text(sc_if, ", then");

    int sc_else_if = block_register("else_if", BLOCKTYPE_CONTROLEND, (Color) { 0xff, 0x99, 0x00, 0xff });
    block_add_text(sc_else_if, "Else if");
    block_add_argument(sc_else_if, "", BLOCKCONSTR_UNLIMITED);
    block_add_text(sc_else_if, ", then");

    int sc_else = block_register("else", BLOCKTYPE_CONTROLEND, (Color) { 0xff, 0x99, 0x00, 0xff });
    block_add_text(sc_else, "Else");

    int sc_end = block_register("end", BLOCKTYPE_END, (Color) { 0x77, 0x77, 0x77, 0xff });
    block_add_text(sc_end, "End");

    int sc_plus = block_register("plus", BLOCKTYPE_NORMAL, (Color) { 0x00, 0xcc, 0x77, 0xFF });
    block_add_argument(sc_plus, "9", BLOCKCONSTR_UNLIMITED);
    block_add_text(sc_plus, "+");
    block_add_argument(sc_plus, "10", BLOCKCONSTR_UNLIMITED);

    int sc_less = block_register("less", BLOCKTYPE_NORMAL, (Color) { 0x00, 0xcc, 0x77, 0xFF });
    block_add_argument(sc_less, "9", BLOCKCONSTR_UNLIMITED);
    block_add_text(sc_less, "<");
    block_add_argument(sc_less, "11", BLOCKCONSTR_UNLIMITED);

    int sc_decl_var = block_register("decl_var", BLOCKTYPE_NORMAL, (Color) { 0xff, 0x66, 0x00, 0xff });
    block_add_text(sc_decl_var, "Declare");
    block_add_argument(sc_decl_var, "my variable", BLOCKCONSTR_STRING);
    block_add_text(sc_decl_var, "=");
    block_add_argument(sc_decl_var, "", BLOCKCONSTR_UNLIMITED);

    int sc_get_var = block_register("get_var", BLOCKTYPE_NORMAL, (Color) { 0xff, 0x66, 0x00, 0xff });
    block_add_argument(sc_get_var, "my variable", BLOCKCONSTR_STRING);

    mouse_blockchain = blockchain_new();
    sprite_code = vector_create();

    sidebar = vector_create();
    for (vec_size_t i = 0; i < vector_size(registered_blocks); i++) {
        if (registered_blocks[i].hidden) continue;
        vector_add(&sidebar, block_new(i));
    }

    igui.is_fading = true;
    igui.elements = vector_create();
    igui.select_element = -1;
}

void free_registered_blocks(void) {
    for (ssize_t i = (ssize_t)vector_size(registered_blocks) - 1; i >= 0 ; i--) {
        block_unregister(i);
    }
    vector_free(registered_blocks);
}

int main(void) {
    set_default_config();

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 600, "Scrap");
    SetTargetFPS(60);
    //EnableEventWaiting();
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    setup();

    while (!WindowShouldClose()) {
        hover_info.sidebar = GetMouseX() < conf.side_bar_size && GetMouseY() > conf.font_size * 2;
        hover_info.block = NULL;
        hover_info.argument = NULL;
        hover_info.argument_pos.x = 0;
        hover_info.argument_pos.y = 0;
        hover_info.prev_argument = NULL;
        hover_info.blockchain = NULL;
        hover_info.blockchain_index = -1;
        hover_info.blockchain_layer = 0;
        hover_info.dropdown_hover_ind = -1;
        hover_info.top_bars.ind = -1;

        Vector2 mouse_pos = GetMousePosition();
        if ((int)hover_info.last_mouse_pos.x == (int)mouse_pos.x && (int)hover_info.last_mouse_pos.y == (int)mouse_pos.y) {
            hover_info.time_at_last_pos += GetFrameTime();
        } else {
            hover_info.last_mouse_pos = mouse_pos;
            hover_info.time_at_last_pos = 0;
        }

        dropdown_check_collisions();
        if (!igui.shown) {
            check_block_collisions();
            bars_check_collisions();
        }
        igui_tick();

        if (GetMouseWheelMove() != 0.0) {
            handle_mouse_wheel();
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            hover_info.drag_cancelled = handle_mouse_click();
#ifdef DEBUG
            // This will traverse through all blocks in codebase, which is expensive in large codebase.
            // Ideally all functions should not be broken in the first place. This helps with debugging invalid states
            sanitize_links();
#endif
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            hover_info.mouse_click_pos = GetMousePosition();
            camera_click_pos = camera_pos;
        } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            handle_mouse_drag();
        } else {
            igui.select_element = -1;
            hover_info.drag_cancelled = false;
            handle_key_press();
        }

        if (IsWindowResized()) {
            shader_time = 0.0;
        } 

        mouse_blockchain.pos = GetMousePosition();

        actionbar.show_time -= GetFrameTime();
        if (actionbar.show_time < 0) actionbar.show_time = 0;

        if (shader_time_loc != -1) SetShaderValue(line_shader, shader_time_loc, &shader_time, SHADER_UNIFORM_FLOAT);
        shader_time += GetFrameTime() / 2.0;
        if (shader_time >= 1.0) shader_time = 1.0;

        // I have no idea why, but this code may occasionally crash X server, so it is turned off for now
        /*if (hover_info.argument || hover_info.select_argument) {
            SetMouseCursor(MOUSE_CURSOR_IBEAM);
        } else if (hover_info.block) {
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }*/

        BeginDrawing();
        ClearBackground(GetColor(0x202020ff));

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        draw_dots();

        DrawRectangle(0, 0, sw, conf.font_size * 1.2, (Color){ 0x30, 0x30, 0x30, 0xFF });
        DrawRectangle(0, conf.font_size * 1.2, sw, conf.font_size, (Color){ 0x2B, 0x2B, 0x2B, 0xFF });
        draw_tab_buttons(sw);
        draw_top_bar();

        BeginScissorMode(0, conf.font_size * 2, sw, sh - conf.font_size * 2);
            for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
                draw_block_chain(&sprite_code[i], camera_pos);
            }
        EndScissorMode();

        BeginScissorMode(0, conf.font_size * 2, conf.side_bar_size, sh - conf.font_size * 2);
            DrawRectangle(0, conf.font_size * 2.2, conf.side_bar_size, sh - conf.font_size * 2.2, (Color){ 0, 0, 0, 0x60 });

            int pos_y = conf.font_size * 2.2 + 10;
            for (vec_size_t i = 0; i < vector_size(sidebar); i++) {
                draw_block((Vector2){ 10, pos_y }, &sidebar[i], true);
                pos_y += conf.font_size + 10;
            }

        EndScissorMode();

        BeginScissorMode(0, conf.font_size * 2, sw, sh - conf.font_size * 2);
            draw_block_chain(&mouse_blockchain, (Vector2) {0});
        EndScissorMode();

        draw_action_bar();

#ifdef DEBUG
        DrawTextEx(
            font_cond, 
            TextFormat(
                "BlockChain: %p, Ind: %d, Layer: %d\n"
                "Block: %p, Parent: %p\n"
                "Argument: %p, Pos: (%.3f, %.3f)\n"
                "Prev argument: %p\n"
                "Select block: %p\n"
                "Select arg: %p, Pos: (%.3f, %.3f)\n"
                "Sidebar: %d\n"
                "Mouse: %p, Time: %.3f, Pos: (%d, %d), Click: (%d, %d)\n"
                "Camera: (%.3f, %.3f), Click: (%.3f, %.3f)\n"
                "Dropdown ind: %d, Scroll: %d\n"
                "Drag cancelled: %d\n"
                "Bar: %d, Ind: %d",
                hover_info.blockchain,
                hover_info.blockchain_index,
                hover_info.blockchain_layer,
                hover_info.block,
                hover_info.block ? hover_info.block->parent : NULL,
                hover_info.argument, hover_info.argument_pos.x, hover_info.argument_pos.y, 
                hover_info.prev_argument,
                hover_info.select_block,
                hover_info.select_argument, hover_info.select_argument_pos.x, hover_info.select_argument_pos.y, 
                hover_info.sidebar,
                mouse_blockchain.blocks,
                hover_info.time_at_last_pos,
                (int)mouse_pos.x, (int)mouse_pos.y,
                (int)hover_info.mouse_click_pos.x, (int)hover_info.mouse_click_pos.y,
                camera_pos.x, camera_pos.y, camera_click_pos.x, camera_click_pos.y,
                hover_info.dropdown_hover_ind, dropdown.scroll_amount,
                hover_info.drag_cancelled,
                hover_info.top_bars.type, hover_info.top_bars.ind
            ), 
            (Vector2){ 
                conf.side_bar_size + 5, 
                conf.font_size * 2.2 + 5
            }, 
            conf.font_size * 0.5,
            0.0, 
            GRAY
        );
#endif

        igui_draw_window();

        draw_dropdown_list();
        draw_tooltip();

        EndDrawing();
    }

    blockchain_free(&mouse_blockchain);
    for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
        blockchain_free(&sprite_code[i]);
    }
    vector_free(sprite_code);
    for (vec_size_t i = 0; i < vector_size(sidebar); i++) {
        block_free(&sidebar[i]);
    }
    vector_free(sidebar);
    free_registered_blocks();
    CloseWindow();

    return 0;
}
