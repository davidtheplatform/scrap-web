// TODO:
// - Better collision resolution
// - C-Blocks
// - Swap blocks inside arguments?
// - Dropdown lists

#include "raylib.h"
#include "vec.h"

#include <stdio.h>
#include <stddef.h>
#include <assert.h>

#define ARRLEN(x) (sizeof(x)/sizeof(x[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define BLOCK_TEXT_SIZE (conf.font_size * 0.6)
#define DATA_PATH "data/"
#define BLOCK_PADDING (5.0 * (float)conf.font_size / 32.0)
#define BLOCK_LINE_SIZE (2.0 * (float)conf.font_size / 32.0)
#define BLOCK_STRING_PADDING (10.0 * (float)conf.font_size / 32.0)

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
    void* arguments; // arguments are of void type because of C. That makes the whole codebase a bit garbled, though :P
    Measurement ms;
    struct Block* parent;
} Block;

typedef enum {
    ARGUMENT_TEXT,
    ARGUMENT_BLOCK,
} BlockArgumentType;

typedef union {
    char* text;
    Block block;
} BlockArgumentData;

typedef struct {
    Measurement ms;
    BlockArgumentType type;
    BlockArgumentData data;
} BlockArgument;

typedef struct {
    Vector2 pos;
    Block* blocks;
} BlockChain;

typedef struct {
    bool sidebar;
    BlockChain* blockchain;
    vec_size_t blockchain_index;
    Block* block;
    BlockArgument* argument;
    BlockArgument* prev_argument;
    Block* select_block;
    BlockArgument* select_argument;
} HoverInfo;

char *top_buttons_text[] = {
    "Code",
    "Output",
};

struct Config conf;
Texture2D run_tex;
Font font;
Font font_cond;

Blockdef* registered_blocks;
Block* sidebar; // Vector that contains block prototypes
BlockChain mouse_blockchain = {0};
BlockChain* sprite_code; // List of block chains
HoverInfo hover_info = {0};

void block_update_parent_links(Block* block) {
    BlockArgument* block_args = block->arguments;
    for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
        if (block_args[i].type != ARGUMENT_BLOCK) continue;
        block_args[i].data.block.parent = block;
    }
}

void update_root_nodes(void) {
    for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
        //if (sprite_code[i].next) {
        //    sprite_code[i].next->prev = &sprite_code[i];
        //}
        block_update_parent_links(&sprite_code[i].blocks[0]);
    }
}

void update_measurements(Block* block) {
    if (block->id == -1) return;
    Blockdef blockdef = registered_blocks[block->id];

    block->ms.size.x = BLOCK_PADDING;
    block->ms.size.y = conf.font_size;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        Measurement ms;
        switch (blockdef.inputs[i].type) {
        case INPUT_TEXT_DISPLAY:
            ms = blockdef.inputs[i].data.stext.ms;
            break;
        case INPUT_STRING:
            BlockArgument* block_args = block->arguments;

            switch (block_args[arg_id].type) {
            case ARGUMENT_TEXT:
                Measurement string_ms;
                string_ms.size = MeasureTextEx(font_cond, block_args[arg_id].data.text, BLOCK_TEXT_SIZE, 0.0);
                string_ms.size.x += BLOCK_STRING_PADDING;
                string_ms.size.x = MAX(conf.font_size - BLOCK_LINE_SIZE * 4, string_ms.size.x);

                block_args[arg_id].ms = string_ms;
                ms = string_ms;
                ms.size.y = MAX(conf.font_size - BLOCK_LINE_SIZE * 4, ms.size.y);
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
        case INPUT_IMAGE_DISPLAY:
            ms = blockdef.inputs[i].data.simage.ms;
            break;
        default:
            ms.size = MeasureTextEx(font_cond, "NODEF", BLOCK_TEXT_SIZE, 0.0);
            break;
        }
        ms.size.x += BLOCK_PADDING;

        block->ms.size.x += ms.size.x;
        block->ms.size.y = MAX(block->ms.size.y, ms.size.y + BLOCK_LINE_SIZE * 4);
    }

    if (block->parent) update_measurements(block->parent);
}

Block new_block(int id) {
    Block block;
    block.id = id;
    block.pos = (Vector2) {0};
    block.ms = (Measurement) {0};
    block.arguments = id == -1 ? NULL : vector_create();
    block.parent = NULL;

    if (id != -1) {
        printf("New block\n\tId: %d\n", id);
        Blockdef blockdef = registered_blocks[block.id];
        for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
            if (blockdef.inputs[i].type != INPUT_STRING) continue;
            BlockArgument* arg = vector_add_dst((BlockArgument**)&block.arguments);
            arg->ms = blockdef.inputs[i].data.stext.ms;
            arg->type = ARGUMENT_TEXT;
            arg->data.text = vector_create();

            for (char* pos = blockdef.inputs[i].data.stext.text; *pos; pos++) {
                vector_add(&arg->data.text, *pos);
            }
            vector_add(&arg->data.text, 0);
            printf("\tInput: %s, Size: %ld\n", arg->data.text, vector_size(arg->data.text));
        }
    }

    update_measurements(&block);
    return block;
}

Block copy_block(Block* block) {
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
            arg->data.block = copy_block(&block_args[i].data.block);
            break;
        default:
            assert(false && "Unimplemented argument copy");
            break;
        }
    }

    return new;
}

void free_block(Block* block) {
    printf("Free block id: %d\n", block->id);

    if (block->arguments) {
        for (vec_size_t i = 0; i < vector_size(block->arguments); i++) {
            BlockArgument* block_args = block->arguments;
            switch (block_args[i].type) {
            case ARGUMENT_TEXT:
                vector_free(block_args[i].data.text);
                break;
            case ARGUMENT_BLOCK:
                free_block(&block_args[i].data.block);
                break;
            default:
                assert(false && "Unimplemented argument free");
                break;
            }
        }
        vector_free((BlockArgument*)block->arguments);
    }
}

/*void free_block_next(Block* block) {
    printf("Free next blocks\n");
    Block* cur = block->next;

    while (cur) {
        Block* next = cur->next;
        free_block(cur);
        free(cur);
        cur = next;
    }
}*/

BlockChain new_blockchain() {
    printf("New blockchain\n");
    BlockChain chain;
    chain.pos = (Vector2) {0};
    chain.blocks = vector_create();

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
        free_block(&chain->blocks[i]);
    }
    vector_clear(chain->blocks);
}

void blockchain_join(BlockChain* dst, BlockChain* src) {
    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks));
    for (vec_size_t i = 0; i < vector_size(src->blocks); i++) {
        vector_add(&dst->blocks, src->blocks[i]);
    }
    blockchain_update_parent_links(dst);
    vector_clear(src->blocks);
}

void blockchain_detach(BlockChain* dst, BlockChain* src, vec_size_t pos) {
    assert(pos < vector_size(src->blocks));

    vector_reserve(&dst->blocks, vector_size(dst->blocks) + vector_size(src->blocks) - pos);
    for (vec_size_t i = pos; i < vector_size(src->blocks); i++) {
        vector_add(&dst->blocks, src->blocks[i]);
    }
    blockchain_update_parent_links(dst);
    vector_erase(src->blocks, pos, vector_size(src->blocks) - pos);
}

void blockchain_free(BlockChain* chain) {
    printf("Free blockchain\n");
    blockchain_clear_blocks(chain);
    vector_free(chain->blocks);
}

void argument_set_block(BlockArgument* block_arg, Block block) {
    assert(block_arg->type == ARGUMENT_TEXT);

    vector_free(block_arg->data.text);
    block_arg->type = ARGUMENT_BLOCK;
    block_arg->data.block = block;

    block_update_parent_links(&block_arg->data.block);
    update_measurements(&block_arg->data.block);
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

void block_add_string_input(int block_id, char* defualt_data) {
    Measurement ms = {0};
    ms.size = MeasureTextEx(font_cond, defualt_data, BLOCK_TEXT_SIZE, 0.0);
    ms.size.x += BLOCK_STRING_PADDING;
    ms.size.x = MAX(conf.font_size - BLOCK_LINE_SIZE * 4, ms.size.x);

    BlockInput* input = vector_add_dst(&registered_blocks[block_id].inputs);
    input->type = INPUT_STRING;
    input->data = (BlockInputData) {
        .stext = {
            .text = defualt_data,
            .ms = ms,
        },
    };
}

void block_add_image(int block_id, Texture2D texture) {
    Measurement ms = {0};
    ms.size.x = (float)(conf.font_size - BLOCK_LINE_SIZE * 4) / (float)texture.height * (float)texture.width;
    ms.size.y = conf.font_size - BLOCK_LINE_SIZE * 4;

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

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
            break;
        case INPUT_STRING:
            BlockArgument* block_args = block->arguments;
            width = block_args[arg_id].ms.size.x;
            Rectangle arg_size;

            switch (block_args[arg_id].type) {
            case ARGUMENT_TEXT:
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_LINE_SIZE * 4) * 0.5;
                arg_size.width = block_args[arg_id].ms.size.x;
                arg_size.height = conf.font_size - BLOCK_LINE_SIZE * 4;

                if (CheckCollisionPointRec(GetMousePosition(), arg_size)) {
                    hover_info.argument = &block_args[arg_id];
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

void draw_block(Vector2 position, Block* block) {
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
    //DrawRectangleRounded(block_size, 0.4, 8, ColorBrightness(color, collision ? 0.3 : 0.0));
    DrawRectangleLinesEx(block_size, BLOCK_LINE_SIZE, ColorBrightness(color, collision ? 0.5 : -0.2));
    //DrawRectangleRoundedLines(block_size, 0.4, 8, BLOCK_LINE_SIZE, ColorBrightness(color, collision ? 0.5 : -0.2));
    cursor.x += BLOCK_PADDING;

    int arg_id = 0;
    for (vec_size_t i = 0; i < vector_size(blockdef.inputs); i++) {
        int width = 0;
        BlockInput cur = blockdef.inputs[i];

        switch (cur.type) {
        case INPUT_TEXT_DISPLAY:
            width = cur.data.stext.ms.size.x;
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
            BlockArgument* block_args = block->arguments;
            width = block_args[arg_id].ms.size.x;

            switch (block_args[arg_id].type) {
            case ARGUMENT_TEXT:
                Rectangle arg_size;
                arg_size.x = cursor.x;
                arg_size.y = cursor.y + block_size.height * 0.5 - (conf.font_size - BLOCK_LINE_SIZE * 4) * 0.5;
                arg_size.width = width;
                arg_size.height = conf.font_size - BLOCK_LINE_SIZE * 4;

                DrawRectangleRec(arg_size, WHITE);
                if (&block_args[arg_id] == hover_info.argument || &block_args[arg_id] == hover_info.select_argument) {
                    DrawRectangleLinesEx(arg_size, BLOCK_LINE_SIZE, ColorBrightness(color, &block_args[arg_id] == hover_info.select_argument ? -0.5 : 0.2));
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

                draw_block(block_pos, &block_args[arg_id].data.block);
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
                    cursor.x, 
                    cursor.y + BLOCK_LINE_SIZE * 2,
                }, 
                0.0, 
                (float)(conf.font_size - BLOCK_LINE_SIZE * 4) / (float)cur.data.simage.image.height, 
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

#ifdef DEBUG
    //DrawTextEx(font_cond, TextFormat("Prev: %p", block->prev), (Vector2) { cursor.x + 10, cursor.y + block_size.height * 0.5 - conf.font_size * 0.5 }, conf.font_size * 0.5, 0.0, WHITE);
    if (!block->parent) {
        DrawTextEx(font_cond, TextFormat("%p", block), (Vector2) { cursor.x + 10, cursor.y + block_size.height * 0.5 - conf.font_size * 0.25 }, conf.font_size * 0.5, 0.0, WHITE);
    }
#endif
}

void draw_block_chain(BlockChain* chain) {
    Vector2 pos = chain->pos;
    for (vec_size_t i = 0; i < vector_size(chain->blocks); i++) {
        draw_block(pos, &chain->blocks[i]);
        pos.y += chain->blocks[i].ms.size.y;
    }
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

    if (selected || (CheckCollisionPointRec(GetMousePosition(), rect) && vector_size(mouse_blockchain.blocks) == 0)) {
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

void handle_mouse_click(void) {
    bool mouse_empty = vector_size(mouse_blockchain.blocks) == 0;

    if (hover_info.sidebar) {
        if (hover_info.select_argument) {
            hover_info.select_argument = NULL;
            return;
        }
        if (mouse_empty && hover_info.block) {
            // Pickup block
            blockchain_add_block(&mouse_blockchain, new_block(hover_info.block->id));
        } else if (!mouse_empty) {
            // Drop block
            blockchain_clear_blocks(&mouse_blockchain);
        }
        return;
    }

    if (mouse_empty) {
        if (hover_info.block != hover_info.select_block) {
            hover_info.select_block = hover_info.block;
        }

        if (hover_info.argument != hover_info.select_argument) {
            hover_info.select_argument = hover_info.argument;
            return;
        }

        if (hover_info.select_argument) {
            return;
        }
    }

    if (!mouse_empty) {
        mouse_blockchain.pos = GetMousePosition();
        if (hover_info.argument) {
            // Attach to argument
            printf("Attach to argument\n");
            if (vector_size(mouse_blockchain.blocks) > 1) return;
            mouse_blockchain.blocks[0].parent = hover_info.block;
            argument_set_block(hover_info.argument, copy_block(&mouse_blockchain.blocks[0]));
            blockchain_clear_blocks(&mouse_blockchain);
        } else if (
            hover_info.block && 
            hover_info.blockchain && 
            hover_info.blockchain_index == vector_size(hover_info.blockchain->blocks) - 1 && 
            hover_info.block->parent == NULL
        ) {
            // Attach block
            printf("Attach block\n");
            blockchain_join(hover_info.blockchain, &mouse_blockchain);
        } else {
            // Put block
            printf("Put block\n");
            vector_add(&sprite_code, mouse_blockchain);
            mouse_blockchain = new_blockchain();
        }
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
            }
        }
    }
}

void handle_key_press(void) {
    if (!hover_info.select_argument) return;
    assert(hover_info.select_argument->type == ARGUMENT_TEXT);

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

void check_collisions(void) {
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
            hover_info.blockchain = &sprite_code[i];
            Vector2 pos = hover_info.blockchain->pos;
            for (vec_size_t j = 0; j < vector_size(hover_info.blockchain->blocks); j++) {
                if (hover_info.block) break;
                hover_info.blockchain_index = j;
                block_update_collisions(pos, &hover_info.blockchain->blocks[j]);
                pos.y += hover_info.blockchain->blocks[j].ms.size.y;
            }
        }
    }
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

    int on_start = block_register("on_start", (Color) { 0xff, 0x77, 0x00, 0xFF });
    block_add_text(on_start, "When");
    block_add_image(on_start, run_tex);
    block_add_text(on_start, "clicked");

    int sc_print = block_register("print", (Color) { 0x00, 0xaa, 0x44, 0xFF });
    block_add_text(sc_print, "Print");
    block_add_string_input(sc_print, "Привет, мусороид!");

    int move_steps = block_register("move_steps", (Color) { 0x00, 0x77, 0xff, 0xFF });
    block_add_text(move_steps, "Move");
    block_add_string_input(move_steps, "");
    block_add_text(move_steps, "pixels");

    int sc_plus = block_register("plus", (Color) { 0x00, 0xcc, 0x77, 0xFF });
    block_add_string_input(sc_plus, "9");
    block_add_text(sc_plus, "+");
    block_add_string_input(sc_plus, "10");

    mouse_blockchain = new_blockchain();
    sprite_code = vector_create();

    sidebar = vector_create();
    for (vec_size_t i = 0; i < vector_size(registered_blocks); i++) {
        vector_add(&sidebar, new_block(i));
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
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);

    setup();

    while (!WindowShouldClose()) {
        hover_info.sidebar = GetMouseX() < conf.side_bar_size && GetMouseY() > conf.font_size * 2;
        hover_info.block = NULL;
        hover_info.argument = NULL;
        hover_info.prev_argument = NULL;
        hover_info.blockchain = NULL;
        hover_info.blockchain_index = -1;

        check_collisions();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            handle_mouse_click();
        } else {
            handle_key_press();
        }
        mouse_blockchain.pos = GetMousePosition();

        BeginDrawing();
        ClearBackground(GetColor(0x202020ff));

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        DrawRectangle(0, 0, sw, conf.font_size, (Color){ 0x30, 0x30, 0x30, 0xFF });
        DrawRectangle(0, conf.font_size, sw, conf.font_size, (Color){ 0x2B, 0x2B, 0x2B, 0xFF });

        draw_top_buttons(sw);

#ifdef DEBUG
        DrawTextEx(
            font_cond, 
            TextFormat(
                "BlockChain: %p, Ind: %d\nBlock: %p, Parent: %p\nArgument: %p\nPrev argument: %p\nSelect block: %p\nSelect arg: %p\nSidebar: %d", 
                hover_info.blockchain,
                hover_info.blockchain_index,
                hover_info.block, 
                hover_info.block ? hover_info.block->parent : NULL,
                hover_info.argument, 
                hover_info.prev_argument,
                hover_info.select_block,
                hover_info.select_argument, 
                hover_info.sidebar
            ), 
            (Vector2){ 
                conf.side_bar_size + 5, 
                conf.font_size * 2 + 5
            }, 
            conf.font_size * 0.5,
            0.0, 
            GRAY
        );
#endif

        BeginScissorMode(0, conf.font_size * 2, conf.side_bar_size, sh - conf.font_size * 2);
            DrawRectangle(0, conf.font_size * 2, conf.side_bar_size, sh - conf.font_size * 2, (Color){ 0, 0, 0, 0x40 });

            int pos_y = conf.font_size * 2 + 10;
            for (vec_size_t i = 0; i < vector_size(sidebar); i++) {
                draw_block((Vector2){ 10, pos_y }, &sidebar[i]);
                pos_y += conf.font_size + 10;
            }

        EndScissorMode();

        BeginScissorMode(conf.side_bar_size, conf.font_size * 2, sw - conf.side_bar_size, sh - conf.font_size * 2);
            for (vec_size_t i = 0; i < vector_size(sprite_code); i++) {
                draw_block_chain(&sprite_code[i]);
            }
        EndScissorMode();

        BeginScissorMode(0, conf.font_size * 2, sw, sh - conf.font_size * 2);
            draw_block_chain(&mouse_blockchain);
        EndScissorMode();

        DrawTextEx(font, "Scrap", (Vector2){ 5, conf.font_size * 0.1 }, conf.font_size * 0.8, 0.0, WHITE);

        EndDrawing();
    }

    free_registered_blocks();
    CloseWindow();

    return 0;
}
