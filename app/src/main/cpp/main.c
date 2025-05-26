#define RAYGUI_IMPLEMENTATION
#include "stdlib.h"
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <android/log.h>

#include "raymob.h"
#include "raygui.h"

#define terminal_scroll_thresh 10
#define terminal_power_scroll_thresh 4 // in pixels
#define terminal_power_scroll_power 0.7f //in relation to y_delta
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Raymob", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "Raymob", __VA_ARGS__))

void ShowNotificationFromC() {
    JNIEnv* env = AttachCurrentThread();
    if (!env) {
        LOGE("AttachCurrentThread failed");
        return;
    }

    jobject activity = GetNativeLoaderInstance();
    if (!activity) {
        LOGE("GetNativeLoaderInstance returned NULL");
        return;
    }

    jclass activityClass = (*env)->GetObjectClass(env, activity);
    if (!activityClass) {
        LOGE("Failed to get activity class");
        return;
    }

    // getClassLoader
    jmethodID getClassLoader = (*env)->GetMethodID(env, activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = (*env)->CallObjectMethod(env, activity, getClassLoader);
    if (!classLoader) {
        LOGE("Failed to get ClassLoader from activity");
        return;
    }

    // loadClass
    jclass classLoaderClass = (*env)->FindClass(env, "java/lang/ClassLoader");
    jmethodID loadClass = (*env)->GetMethodID(env, classLoaderClass, "loadClass",
                                              "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring strClassName = (*env)->NewStringUTF(env, "com.raylib.raymob.NativeLoader");
    jclass loaderClass = (jclass)(*env)->CallObjectMethod(env, classLoader, loadClass, strClassName);

    if (!loaderClass) {
        LOGE("Could not load NativeLoader class using ClassLoader");
        return;
    }

    // finally, call the method
    jmethodID notifyMethod = (*env)->GetStaticMethodID(env, loaderClass, "showTestNotification", "()V");
    if (!notifyMethod) {
        LOGE("Could not find method showTestNotification");
        return;
    }

    LOGI("Calling showTestNotification...");
    (*env)->CallStaticVoidMethod(env, loaderClass, notifyMethod);
    LOGI("showTestNotification should have been called.");
}

Image LoadImageFromAssets(const char *filename) {
	Image img = { 0 };

	AAssetManager *mgr = GetAndroidApp()->activity->assetManager;
	AAsset *asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
	if (!asset) {
		TraceLog(LOG_ERROR, "Could not open asset: %s", filename);
		return img;
	}

	int size = AAsset_getLength(asset);
	unsigned char *buffer = (unsigned char *)malloc(size);
	if (!buffer) {
		TraceLog(LOG_ERROR, "Out of memory reading asset: %s", filename);
		AAsset_close(asset);
		return img;
	}

	AAsset_read(asset, buffer, size);
	AAsset_close(asset);

	img = LoadImageFromMemory(GetFileExtension(filename), buffer, size);
	TraceLog(LOG_INFO, "Loaded image size: %d x %d", img.width, img.height);
	free(buffer);
	return img;
}

static inline float lerp_f(float a, float b, float t) { return a + t * (b - a); }


//default colors
const Color DEFAULT_COLOR_1 = {0x08, 0x0D, 0x10, 0xff};
const Color DEFAULT_COLOR_2 = {0x66, 0xCC, 0xE1, 0xff};

// globals
int
	screenWidth,
	screenHeight,
	y_spilit
;


// touch variables
char is_touch_active = 0;
float drag_time  = 0;
Vector2 touch_start_pos = { 0 }, touch_current_pos = {0 };
void (*current_touch_owner)(void) = NULL;


// terminal variables
static char terminal_text_buffer[4096] = "Line 1\nLine 2\nLine 3\nYou get the idea...\nAdd more here...\n";
static Vector2 terminal_scroll = {0};  // terminal_scroll offset
char is_terminal_power_scrolling = 0;
float terminal_power_scroll_intense = 0;
#define terminal_power_scroll_fade 2.0f


void terminal_box_scroll_touch_manager() {
	static char has_saved_the_start = 0;
	static float start = 0;
	static float y_last_frame = 0;

	if (!has_saved_the_start) {
		has_saved_the_start = 1;
		start = terminal_scroll.y;
		y_last_frame = touch_current_pos.y;
	}
	terminal_scroll.y = start + touch_current_pos.y - touch_start_pos.y;

	float delta_y = touch_current_pos.y - y_last_frame;
	if (!is_touch_active) {
		has_saved_the_start = 0;
		if (delta_y > terminal_power_scroll_thresh || delta_y < terminal_power_scroll_thresh){
			is_terminal_power_scrolling = 1;
			terminal_power_scroll_intense = delta_y * terminal_power_scroll_power;
		}
	}
	y_last_frame = touch_current_pos.y;
}


void top_touch_manager() {
	TraceLog(LOG_INFO, "the owner is top section");
	int delta_y = (int)touch_current_pos.y - (int)touch_start_pos.y;
	int dleta_x = touch_current_pos.x - touch_start_pos.x;
	if (delta_y > terminal_scroll_thresh || delta_y < -terminal_scroll_thresh) {
		current_touch_owner = terminal_box_scroll_touch_manager;
		touch_start_pos = touch_current_pos;
		current_touch_owner();
	}
}

void screen_touch_manager() {
	if (touch_current_pos.y < (float)y_spilit) {
		current_touch_owner = top_touch_manager;
		current_touch_owner();
	} else {TraceLog(LOG_INFO, "touched bottom section");}
}

void update_touch_state(void) {
	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
		is_touch_active = 1;
		touch_start_pos = GetMousePosition();
		touch_current_pos = touch_start_pos;
		drag_time = 0.0f;
		current_touch_owner = screen_touch_manager;
	}

	if (is_touch_active) {
		touch_current_pos = GetMousePosition();
		drag_time += GetFrameTime();

		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			is_touch_active = 0;
			if (current_touch_owner) current_touch_owner();
			drag_time = 0.0f;
			current_touch_owner = NULL;
		}
		if (current_touch_owner) current_touch_owner();
	}
}




void draw_scrollable_textbox() {
	Rectangle panelBounds = {0.0f, 0.0f, screenWidth, y_spilit}; // Area for the terminal_scroll panel
	Rectangle contentBounds = {0.0f, 0.0f, screenWidth - 20, 1000}; // Arbitrary large height, can be dynamic

	// Panel with scrollable region
	Rectangle view;
	GuiScrollPanel(panelBounds, NULL, contentBounds, &terminal_scroll, &view);

	// Draw text inside the terminal_scroll view
	BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);

	if (is_terminal_power_scrolling) {
		terminal_scroll.y += terminal_power_scroll_intense;
		terminal_power_scroll_intense = lerp_f(terminal_power_scroll_intense, 0 , GetFrameTime() * terminal_power_scroll_fade);
		if (terminal_power_scroll_intense <= 0.1f && terminal_power_scroll_intense >= -0.1f )
			is_terminal_power_scrolling = 0;
	}
	if(terminal_scroll.y < -(contentBounds.height - panelBounds.height))
		terminal_scroll.y = -(contentBounds.height - panelBounds.height);
	DrawTextEx(GetFontDefault(), terminal_text_buffer,
			   (Vector2){view.x + terminal_scroll.x + 5, view.y + terminal_scroll.y + 5},
			   30, 2, DEFAULT_COLOR_2);
	EndScissorMode();
}


void update_colors(Color c1, Color c2) {
	GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(c1));
	GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(c1));
	GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(c1));
	GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(c1));
	GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(c1));

	GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(c2));
	GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(c2));
	GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(c1));
}

int main(void) {
    InitWindow(0, 0, "raylib [core] example - basic window");
	SetGesturesEnabled(0b111111111);  // Enable all gestures
	SetTargetFPS(60);


    ShowNotificationFromC();
    //--------------------------------------------------------------------------------------

    screenWidth  = GetScreenWidth();
	screenHeight = GetScreenHeight();
	y_spilit = screenHeight / 2 - 2;



    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        BeginDrawing();
		update_touch_state();

        ClearBackground(DEFAULT_COLOR_1);
		update_colors(DEFAULT_COLOR_1, DEFAULT_COLOR_2);

		// drawing the UI
		//y spilit
        DrawRectangle(0, y_spilit, screenWidth, 5, DEFAULT_COLOR_2);
		// text box
		draw_scrollable_textbox();



        EndDrawing();
    }


    CloseWindow();
    return 0;
}


