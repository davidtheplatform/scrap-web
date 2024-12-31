#include "tinyfiledialogs.h"

#include <emscripten/emscripten.h>

char tinyfd_version[8] = "0.1.0w";
char tinyfd_needs[] = "Must be compiled with emscripten and be run in a web browser";
int tinyfd_verbose = 0;
int tinyfd_silent = 1;

int tinyfd_allowCursesDialogs = 0;
int tinyfd_forceConsole = 0;

int tinyfd_assumeGraphicDisplay = 1;

char tinyfd_response[1024];

void EMSCRIPTEN_KEEPALIVE tinyfd_beep(void)
{
    // TODO
    return;
}

int EMSCRIPTEN_KEEPALIVE tinyfd_notifyPopup(char const *aTitle, char const *aMessage, char const *aIconType)
{
    EM_ASM({alert(UTF8ToString($0) + '\n' + UTF8ToString($1))}, aTitle, aMessage);
    return 0;
}

int EMSCRIPTEN_KEEPALIVE tinyfd_messageBox(char const *aTitle, char const *aMessage, char const *aDialogType, char const *aIconType, int aDefaultButton)
{
    // cancel is not supported
    return EM_ASM_INT({
            result = confirm(UTF8ToString($0) + '\n' + UTF8ToString($1));
            if (result) return 1;
            return 0;
        },
        aTitle, aMessage);
}

char* EMSCRIPTEN_KEEPALIVE tinyfd_inputBox(
    char const *aTitle,
    char const *aMessage,
    char const *aDefaultInput) {
        return EM_ASM_PTR({
            return prompt(UTF8ToString($0) + '\n' + UTF8ToString($1), $2);
        },
        aTitle, aMessage, aDefaultInput);
    }

char * EMSCRIPTEN_KEEPALIVE tinyfd_saveFileDialog(
	char const * aTitle , /* NULL or "" */
	char const * aDefaultPathAndOrFile , /* NULL or "" , ends with / to set only a directory */
	int aNumOfFilterPatterns , /* 0  (1 in the following example) */
	char const * const * aFilterPatterns , /* NULL or char const * lFilterPatterns[1]={"*.txt"} */
	char const * aSingleFilterDescription ) {
        // TODO
        return NULL;
    }

char * EMSCRIPTEN_KEEPALIVE tinyfd_openFileDialog(
	char const * aTitle, /* NULL or "" */
	char const * aDefaultPathAndOrFile, /* NULL or "" , ends with / to set only a directory */
	int aNumOfFilterPatterns , /* 0 (2 in the following example) */
	char const * const * aFilterPatterns, /* NULL or char const * lFilterPatterns[2]={"*.png","*.jpg"}; */
	char const * aSingleFilterDescription, /* NULL or "image files" */
	int aAllowMultipleSelects ) {
        // TODO
        return NULL;
    }

char * EMSCRIPTEN_KEEPALIVE tinyfd_selectFolderDialog(
	char const * aTitle, /* NULL or "" */
	char const * aDefaultPath) {
        // TODO
        return NULL;
    }

char * EMSCRIPTEN_KEEPALIVE tinyfd_colorChooser(
	char const * aTitle, /* NULL or "" */
	char const * aDefaultHexRGB, /* NULL or "" or "#FF0000" */
	unsigned char const aDefaultRGB[3] , /* unsigned char lDefaultRGB[3] = { 0 , 128 , 255 }; */
	unsigned char aoResultRGB[3] ) {
        // TODO
        return NULL;
    }