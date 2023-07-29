#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <luajit-2.1/lauxlib.h>

#include <cairo/cairo.h>

#include <glib-2.0/glib.h>
#include <gtk-3.0/gdk/gdk.h>

#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

typedef long long int i64;
typedef long int i32;
typedef int i16;
typedef char i8;

typedef unsigned long long int u64;
typedef unsigned long int u32;
typedef unsigned int u16;
typedef unsigned char u8;

#define Surface cairo_surface_t

#define def(name) static int name(lua_State* L)

#define new(type) malloc(sizeof(type))

#define for_range(loopvar, stop) for (u8 loopvar = 0; loopvar < stop; loopvar++)

#define print(fmt, ...) printf(("\x1b[1;34mc_filter -> \x1b[0m" fmt "\n"), __VA_ARGS__)

typedef struct Pixel {
	u8 red, green, blue, alpha;
} __attribute__((packed)) Pixel;

#define index_color(pixel, index) ((u8*)(pixel))[index]

#define auto_pointer(type, name, size, value, block) { type* name = malloc(sizeof(type) * size); name = value; block; free(name); }

/*
// (surface: cairo.Surface, width: integer, height: integer) -> GdkPixbuf.Pixbuf
def(export_surface_to_pixbuf) {
	Surface* surface = (Surface*)lua_touserdata(L, 1);
	int width = (i64)luaL_checknumber(L, 2);
	int height = (i64)luaL_checknumber(L, 3);

	GdkPixbuf* pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);

	lua_pushlightuserdata(L, pixbuf);

	return 1;
}
*/

double clamp(double number, double floor, double ceiling) {
	if (number < floor) {
		return floor;
	} else if (number > ceiling) {
		return ceiling;
	} else {
		return number;
	}
}

i64 int_clamp(i64 number, i64 floor, i64 ceiling) {
	return (i64)clamp(number, floor, ceiling);
}

// (number: number, floor: number, ceiling: number) -> number
def(export_clamp) {
	double number  = luaL_checknumber(L, 1);
	double floor   = luaL_checknumber(L, 2);
	double ceiling = luaL_checknumber(L, 3);

	double result = clamp(number, floor, ceiling);

	lua_pushnumber(L, result);

	return 1;
}

const double pi = 3.1415926535897932384626433832795;
// Based on https://stackoverflow.com/a/8204867
double* generate_blur_kernel(i32 radius, i32 sigma) {
	double* kernel = malloc(sizeof(double) * (radius * radius));
	double mean = radius / 2;
	double sum = 0.0;

	for_range (x, radius) {
		for_range (y, radius) {
			i16 index = x + (y * radius);

			kernel[index] = exp(
				-0.5 * (pow((x - mean) / sigma, 2.0) + pow((y - mean) / sigma, 2.0))
			) / (
				2 * pi * sigma * sigma
			);

			sum += kernel[index];
		}
	}

	for_range (x, radius) {
		for_range (y, radius) {
			kernel[x + (y * radius)] /= sum;
		}
	}

	return kernel;
}

GdkPixbuf* blur_pixbuf(GdkPixbuf* pixbuf, const i32 radius, const bool no_use_kernel) {
	guint* _length = new(guint);

	Pixel* pixels = (Pixel*)gdk_pixbuf_get_pixels_with_length(pixbuf, _length);

	const u32 length = *_length / 4;
	free(_length);

	const u32 rowstride = gdk_pixbuf_get_rowstride(pixbuf); 
	const u32 width     = gdk_pixbuf_get_width(pixbuf);
	const u32 height    = gdk_pixbuf_get_height(pixbuf);

	const u16 half = ceil(0.5 * radius);

	const double multiplier = 1.0f / (double)(radius * radius);

	Pixel* blurred_pixels = malloc(sizeof(Pixel) * length);

	double* kernel = generate_blur_kernel(radius, (radius / 2.0));

	for_range (y, height) {
		for_range (x, width) {

			u32 index = x + (y * height);

			blurred_pixels[index].alpha = 0;
			blurred_pixels[index].red   = 0;
			blurred_pixels[index].green = 0;
			blurred_pixels[index].blue  = 0;

			u32 y_floor = clamp(y - half, 0, height);
			u32 y_ceil  = clamp(y + half, 0, height);
			u32 x_floor = clamp(x - half, 0, width);
			u32 x_ceil  = clamp(x + half, 0, width);

			for_range (channel_offset, 4) {
				u32 color_index = index * 4 + channel_offset;

				double current_color = 0; // index_color(blurred_pixels, color_index);

				if (no_use_kernel) {
					for (i32 y_offset = y_floor; y_offset < y_ceil; y_offset++) {
						for (i32 x_offset = x_floor; x_offset < x_ceil; x_offset++) {
							current_color += (double)index_color(pixels, ((x_offset + (y_offset * height)) * 4 + channel_offset));
						}
					}

					current_color *= multiplier;
				} else {
					for_range (y_offset, radius) {
						for_range (x_offset, radius) {
							double kernel_value = kernel[x_offset + (y_offset * radius)];
							current_color += index_color(
								pixels,
								((int_clamp(x + (x_offset - half), 0, width) + ((int_clamp(y + (y_offset - half), 0, height)) * height)) * 4 + channel_offset)
							) * kernel_value;
						}
					}
				}

				index_color(blurred_pixels, color_index) = (u8)current_color;
			}
		}
	}

	GdkPixbuf* blurred_pixbuf = gdk_pixbuf_new_from_data(
		(u8*)blurred_pixels,
		GDK_COLORSPACE_RGB,
		true,
		8,
		width,
		height,
		rowstride,
		NULL,
		NULL
	);

	free(kernel);

	return blurred_pixbuf;
}

def(export_blur_pixbuf) {
	GdkPixbuf* pixbuf  = lua_touserdata(L, 1);
	i32 radius         = luaL_checkint(L, 2);
	bool no_use_kernel = lua_toboolean(L, 3);

	GdkPixbuf* blurred_pixbuf = blur_pixbuf(pixbuf, radius, no_use_kernel);

	lua_pushlightuserdata(L, blurred_pixbuf);

	return 1;
}

typedef enum BlendingMode {
	ADD
} BlendingMode;

// Note: pixbuf_a and pixbuf_b MUST have the same dimensions
GdkPixbuf* combine_pixbufs(GdkPixbuf* pixbuf_a, GdkPixbuf* pixbuf_b, const BlendingMode blending_mode) {
	guint* _length = new(guint);

	u8* pixels_a = gdk_pixbuf_get_pixels_with_length(pixbuf_a, _length);

	const u32 length    = *_length;
	const u32 rowstride = gdk_pixbuf_get_rowstride(pixbuf_a);
	const u32 width     = gdk_pixbuf_get_width(pixbuf_a);
	const u32 height    = gdk_pixbuf_get_height(pixbuf_a);

	u8* pixels_b = gdk_pixbuf_get_pixels_with_length(pixbuf_b, _length);

	free(_length);

	u8* combined_pixels = malloc(sizeof(u8) * length);

	switch (blending_mode) {
		case ADD:
			for (u32 i = 0; i < length; i++) {
				combined_pixels[i] = clamp(pixels_a[i] + pixels_b[i], 0, 255);
			}

			break;
	}

	GdkPixbuf* combined_pixbuf = gdk_pixbuf_new_from_data(
		combined_pixels,
		GDK_COLORSPACE_RGB,
		true,
		8,
		width,
		height,
		rowstride,
		NULL,
		NULL
	);

	return combined_pixbuf;
}

def(export_combine_pixbufs) {
	GdkPixbuf* pixbuf_a = lua_touserdata(L, 1);
	GdkPixbuf* pixbuf_b = lua_touserdata(L, 2);
	BlendingMode mode   = luaL_checkint(L, 3);

	GdkPixbuf* combined_pixbuf = combine_pixbufs(pixbuf_a, pixbuf_b, mode);

	lua_pushlightuserdata(L, combined_pixbuf);

	return 1;
}

GdkPixbuf* tint_pixbuf(GdkPixbuf* pixbuf, const u8 red, const u8 green, const u8 blue) {
	guint* _length = new(guint);

	Pixel* pixels = (Pixel*)gdk_pixbuf_get_pixels_with_length(pixbuf, _length);

	const u32 length    = *_length / 4;
	const u32 rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	const u32 width     = gdk_pixbuf_get_width(pixbuf);
	const u32 height    = gdk_pixbuf_get_height(pixbuf);

	free(_length);

	Pixel* tinted_pixels = malloc(sizeof(Pixel) * length);

	for (u32 i = 0; i < length; i++) {
		tinted_pixels[i].red   = red;
		tinted_pixels[i].green = green;
		tinted_pixels[i].blue  = blue;
		tinted_pixels[i].alpha = pixels[i].alpha;
	}

	GdkPixbuf* tinted_pixbuf = gdk_pixbuf_new_from_data(
		(u8*)tinted_pixels,
		GDK_COLORSPACE_RGB,
		true,
		8,
		width,
		height,
		rowstride,
		NULL,
		NULL
	);

	return tinted_pixbuf;
}

def(export_tint_pixbuf) {
	GdkPixbuf* pixbuf = lua_touserdata(L, 1);
	u8 red   = luaL_checkint(L, 2);
	u8 green = luaL_checkint(L, 3);
	u8 blue  = luaL_checkint(L, 4);

	GdkPixbuf* tinted_pixbuf = tint_pixbuf(pixbuf, red, green, blue);

	lua_pushlightuserdata(L, tinted_pixbuf);

	return 1;
}

GdkPixbuf* apply_multiplier_to_pixbuf(GdkPixbuf* pixbuf, const double red, const double green, const double blue, const double alpha) {
	guint* _length = new(guint);

	Pixel* pixels = (Pixel*)gdk_pixbuf_get_pixels_with_length(pixbuf, _length);

	const u32 length    = *_length / 4;
	const u32 rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	const u32 width     = gdk_pixbuf_get_width(pixbuf);
	const u32 height    = gdk_pixbuf_get_height(pixbuf);

	free(_length);

	Pixel* tinted_pixels = malloc(sizeof(Pixel) * length);

	for (u32 i = 0; i < length; i++) {
		tinted_pixels[i].red   = clamp(pixels[i].red   * red,   0, 255);
		tinted_pixels[i].green = clamp(pixels[i].green * green, 0, 255);
		tinted_pixels[i].blue  = clamp(pixels[i].blue  * blue,  0, 255);
		tinted_pixels[i].alpha = clamp(pixels[i].alpha * alpha, 0, 255);
	}

	GdkPixbuf* tinted_pixbuf = gdk_pixbuf_new_from_data(
		(u8*)tinted_pixels,
		GDK_COLORSPACE_RGB,
		true,
		8,
		width,
		height,
		rowstride,
		NULL,
		NULL
	);

	return tinted_pixbuf;
}

GdkPixbuf* add_shadow_to_pixbuf(GdkPixbuf* pixbuf, u32 radius, double opcaity) {
	GdkPixbuf* blurred_pixbuf = blur_pixbuf(pixbuf, radius, true);

	GdkPixbuf* tinted_pixbuf = apply_multiplier_to_pixbuf(blurred_pixbuf, 0.0, 0.0, 0.0, 0.75);
	g_free(blurred_pixbuf);

	GdkPixbuf* shaded_pixbuf = combine_pixbufs(pixbuf, tinted_pixbuf, ADD);
	g_free(tinted_pixbuf);

	return shaded_pixbuf;
}

def(export_add_shadow_to_pixbuf) {
	GdkPixbuf* pixbuf = lua_touserdata(L, 1);
	u32 radius = luaL_checkint(L, 2);
	double opcaity = lua_tonumber(L, 3);

	GdkPixbuf* shaded_pixbuf = add_shadow_to_pixbuf(pixbuf, radius, opcaity);

	lua_pushlightuserdata(L, shaded_pixbuf);

	return 1;
}

static const struct luaL_Reg filters[] = {
	//{ "surface_to_pixbuf",    export_surface_to_pixbuf    },
	{ "clamp",                export_clamp                },
	{ "blur_pixbuf",          export_blur_pixbuf          },
	{ "combine_pixbufs",      export_combine_pixbufs      },
	{ "tint_pixbuf",          export_tint_pixbuf          },
	{ "add_shadow_to_pixbuf", export_add_shadow_to_pixbuf },
	{ NULL, NULL }
};

int luaopen_filters(lua_State* L) {
	luaL_newlib(L, filters);

	return 1;
}
