#include "map.h"

#include "renderer.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef enum {
	TILE_HORIZONTAL = 0,
	TILE_VERTICAL,
	TILE_UPPERLEFT,
	TILE_UPPERRIGHT,
	TILE_BOTTOMLEFT,
	TILE_BOTTOMRIGHT,
	TILE_NONE
} TileType;

typedef struct
{
	AreaType type;
	SDL_Rect rect;
	SDL_Texture *texture;
} Modifier;


static SDL_Texture *tile_horizontal = 0;
static SDL_Texture *tile_vertical   = 0;
static SDL_Texture *tile_upperleft  = 0;
static SDL_Texture *tile_upperright = 0;
static SDL_Texture *tile_lowerleft  = 0;
static SDL_Texture *tile_lowerright = 0;
static SDL_Texture *tile_none       = 0;

static SDL_Texture *mod_booster_texture = 0;
static SDL_Texture *mod_ice_texture     = 0;
static SDL_Texture *mod_mud_texture     = 0;
static SDL_Texture *mod_oil_texture     = 0;
static SDL_Texture *mod_banana_texture  = 0;

static TileType **map_tiles = 0;

static unsigned int height, width;

static const unsigned int MODIFIER_WIDTH = 64;
static const unsigned int MODIFIER_HEIGHT = 64;
static int modifiers_count = 0;
static int modifiers_size = 0;
static Modifier *modifiers = 0;

int boxlocations_count = 0;
ivec2 *boxlocations = 0;

ivec2 map_starting_position;

#define MAX_PATH_LENGTH 100
ivec2 map_path[MAX_PATH_LENGTH];
int map_path_length = 0;

int map_laps = 4;

void remove_modifier(int index)
{
	modifiers_count--;
	for (int i=index; i<modifiers_count; i++) {
		modifiers[i] = modifiers[i+1];
	}
}

int map_add_modifier(AreaType type, ivec2 pos)
{
	if (modifiers_size < modifiers_count) {
		modifiers_size += 10; // grow linearly, makes more sense for this
		modifiers = realloc(modifiers, modifiers_size * sizeof(Modifier));
		if (!modifiers) {
			printf("failed to grow array for modifiers\n");
			return 1;
		}
	}

	Modifier modifier;
	modifier.rect.x = pos.x;
	modifier.rect.y = pos.y;
	modifier.rect.w = MODIFIER_WIDTH;
	modifier.rect.h = MODIFIER_HEIGHT;
	modifier.type = type;
	switch (type) {
	case MAP_BOOST:
		modifier.texture = mod_booster_texture;
		modifier.rect.w /= 2;
		modifier.rect.h /= 2;
		break;
	case MAP_ICE:
		modifier.texture = mod_ice_texture;
		break;
	case MAP_MUD:
		modifier.texture = mod_mud_texture;
		break;
	case MAP_OIL:
		modifier.texture = mod_oil_texture;
		break;
	case MAP_BANANA:
		modifier.texture = mod_banana_texture;
		break;
	default:
		printf("invalid modifier type: %d\n", type);
		return 1;
	}

	modifiers[modifiers_count] = modifier;

	modifiers_count++;

	return 0;
}

int add_box_location(ivec2 pos)
{
	ivec2 boxlocation;
	boxlocation.x = pos.x;
	boxlocation.y = pos.y;
	boxlocations[boxlocations_count] = boxlocation;
	boxlocations_count++;
	return 0;
}

int map_load_tiles();
int map_load_file(const char *file);

int map_init(const char *map_file)
{
	if (!map_load_tiles()) {
		printf("failed to load tiles\n");
		return 0;
	}

	if (!map_load_file(map_file)) {
		printf("failed to load map file\n");
		return 0;
	}

	return 1;
}

void map_destroy()
{
	for (unsigned int x = 0; x < width; x++) {
		free(map_tiles[x]);
	}
	free(map_tiles);
}

static int get_r(unsigned int x, unsigned int y, unsigned int ox, unsigned int oy)
{
	unsigned int w = abs(x - ox);
	unsigned int h = abs(y - oy);
	return sqrtl(w*w + h*h);
}

AreaType map_get_type(const ivec2 pos)
{
	if (pos.x < 0 || pos.y < 0) {
		return MAP_WALL;
	}

	for (int i = 0; i < modifiers_count; i++) {
		const int mod_min_x = modifiers[i].rect.x;
		const int mod_min_y = modifiers[i].rect.y;
		const int mod_max_x = mod_min_x + modifiers[i].rect.w;
		const int mod_max_y = mod_min_y + modifiers[i].rect.h;
		if (pos.x > mod_min_x && pos.x < mod_max_x && pos.y > mod_min_y && pos.y < mod_max_y) {
			if (modifiers[i].type == MAP_BANANA) {
				remove_modifier(i);
			}
			return modifiers[i].type;
		}
	}

	const unsigned int px = pos.x / TILE_WIDTH;
	const unsigned int py = pos.y / TILE_HEIGHT;
	if (px >= width || py >= height) {
		return MAP_WALL;
	}

	unsigned int rel_x = pos.x - (px * TILE_WIDTH);
	unsigned int rel_y = pos.y - (py * TILE_HEIGHT);

	int distance;
	switch (map_tiles[px][py]) {
	case TILE_HORIZONTAL:
		distance = rel_y;
		break;
	case TILE_VERTICAL:
		distance = rel_x;
		break;
	case TILE_UPPERLEFT:
		distance = get_r(rel_x, rel_y, TILE_WIDTH, TILE_HEIGHT);
		break;
	case TILE_UPPERRIGHT:
		distance = get_r(rel_x, rel_y, 0, TILE_HEIGHT);
		break;
	case TILE_BOTTOMLEFT:
		distance = get_r(rel_x, rel_y, TILE_WIDTH, 0);
		break;
	case TILE_BOTTOMRIGHT:
		distance = get_r(rel_x, rel_y, 0, 0);
		break;
	case TILE_NONE:
	default:
		return MAP_GRASS;
	}

	if (distance < 16 || distance > 112) {
		return MAP_GRASS;
	} else {
		return MAP_TRACK;
	}
	return MAP_TRACK;
}

int map_load_tiles()
{
	tile_horizontal = ren_load_image("map-horizontal.bmp");
	tile_vertical   = ren_load_image("map-vertical.bmp");
	tile_upperleft  = ren_load_image("map-topleft.bmp");
	tile_upperright = ren_load_image("map-topright.bmp");
	tile_lowerleft  = ren_load_image("map-bottomleft.bmp");
	tile_lowerright = ren_load_image("map-bottomright.bmp");
	tile_none       = ren_load_image("map-none.bmp");

	mod_booster_texture = ren_load_image("booster.bmp");
	mod_ice_texture     = ren_load_image("ice.bmp");
	mod_mud_texture     = ren_load_image("mud.bmp");
	mod_oil_texture     = ren_load_image("oil_spill.bmp");
	mod_banana_texture  = ren_load_image("placed_banana.bmp");

	return (tile_horizontal &&
		tile_vertical   &&
		tile_upperleft  &&
		tile_upperright &&
		tile_lowerleft  &&
		tile_lowerright &&
		tile_none       &&
		mod_booster_texture &&
		mod_ice_texture     &&
		mod_mud_texture     &&
		mod_oil_texture     &&
		mod_banana_texture
		);
}

int map_load_file(const char *filename)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		printf("failed to open map file %s\n", filename);
		return 0;
	}

	if (fscanf(file, "%ux%u\n", &width, &height) != 2) {
		printf("invalid map file format in file %s\n", filename);
		fclose(file);
		return 0;
	}

	map_tiles = malloc((width + 1) * sizeof(TileType*));
	if (!map_tiles) {
		printf("failed to allocate %lu bytes for map tiles\n", height * sizeof(TileType*));
		return 0;
	}
	for (unsigned int x = 0; x < width; x++) {
		map_tiles[x] = malloc((height + 1) * sizeof(TileType));

		if (!map_tiles[x]) {
			printf("failed to allocate %lu bytes of memory for map tile line\n", height * sizeof(TileType*));
		}
	}

	char buf[128] = { 0 };
	for (unsigned int y=0; y<height; y++) {
		if (fgets(buf, sizeof(buf), file) == NULL || strlen(buf) < width) {
			printf("line length %lu (of %s) is less than map width %u\n", strlen(buf), buf, width);
			return 0;
		}

		for (unsigned int x=0; x<width; x++) {
			switch(buf[x]) {
			case '-':
				map_tiles[x][y] = TILE_HORIZONTAL;
				break;
			case '|':
				map_tiles[x][y] = TILE_VERTICAL;
				break;
			case '/':
				map_tiles[x][y] = TILE_UPPERLEFT;
				break;
			case '`':
				map_tiles[x][y] = TILE_UPPERRIGHT;
				break;
			case '\\':
				map_tiles[x][y] = TILE_BOTTOMLEFT;
				break;
			case ',':
				map_tiles[x][y] = TILE_BOTTOMRIGHT;
				break;
			case '.':
				map_tiles[x][y] = TILE_NONE;
				break;
			default:
				printf("invalid map tile type %c at x: %u y: %u\n", buf[x], x, y);
				return 0;
			}
		}
	}

	ivec2 starting_tile;
	if (fscanf(file, "%d,%d\n", &starting_tile.x, &starting_tile.y) != 2) {
		printf("unable to read starting position from map file!\n");
		fclose(file);
		return 0;
	}
	map_starting_position.x = TILE_WIDTH * (starting_tile.x) + TILE_WIDTH / 2;
	map_starting_position.y = TILE_HEIGHT * (starting_tile.y) + TILE_HEIGHT / 5;

	if (fscanf(file, "%d\n", &modifiers_size) != 1) {
		printf("unable to read amount of modifiers\n");
		fclose(file);
		return 0;
	}

	modifiers = malloc(sizeof(Modifier) * (modifiers_size + 1));
	if (!modifiers) {
		printf("failed to allocate memory for modifiers\n");
		return 0;
	}
	ivec2 modifier_pos;
	int ret = 0;
	for (int i=0; i<modifiers_size; i++) {
		if (fscanf(file, "%d %d %s\n", &modifier_pos.x, &modifier_pos.y, buf) != 3) {
			printf("Unable to read modifier!\n");
			fclose(file);
			return 0;
		}
		if (strcmp(buf, "mud") == 0) {
			ret = map_add_modifier(MAP_MUD, modifier_pos);
		} else if (strcmp(buf, "ice") == 0) {
			ret = map_add_modifier(MAP_ICE, modifier_pos);
		} else if (strcmp(buf, "booster") == 0) {
			ret = map_add_modifier(MAP_BOOST, modifier_pos);
		}
		if (ret) {
			printf("error while adding modifier\n");
			return 0;
		}
	}


	int boxlocations_size;
	if (fscanf(file, "%d\n", &boxlocations_size) != 1) {
		printf("unable to read amount of box locations!\n");
		fclose(file);
		return 0;
	}

	boxlocations = malloc(sizeof(ivec2) * (boxlocations_size + 1));
	if (!boxlocations) {
		printf("failed to allocate memory for box locations\n");
		return 0;
	}

	ivec2 box_location;
	for (int i=0; i<boxlocations_size; i++) {
		if (fscanf(file, "%d %d\n", &box_location.x, &box_location.y) != 2) {
			printf("unable to read box location from file!");
			return 0;
		}
		if (add_box_location(box_location)) {
			printf("error while adding box location\n");
			return 0;
		}
	}

	rewind(file);
	fclose(file);

	// Find path through map

	map_path[0] = starting_tile;
	map_path[1] = starting_tile;
	map_path[1].x++;
	for (int i=1; i<MAX_PATH_LENGTH - 1; i++) {
		ivec2 next_tile = map_path[i];
		switch(map_tiles[next_tile.x][next_tile.y]) {
		case TILE_HORIZONTAL:
			if (map_path[i-1].x == next_tile.x - 1) {
				next_tile.x++;
			} else {
				next_tile.x--;
			}
			break;
		case TILE_VERTICAL:
			if (map_path[i-1].y == next_tile.y - 1) {
				next_tile.y++;
			} else {
				next_tile.y--;
			}
			break;
		case TILE_UPPERLEFT:
			if (map_path[i-1].x == next_tile.x + 1) {
				next_tile.y++;
			} else {
				next_tile.x++;
			}
			break;
		case TILE_UPPERRIGHT:
			if (map_path[i-1].x == next_tile.x - 1) {
				next_tile.y++;
			} else {
				next_tile.x--;
			}
			break;
		case TILE_BOTTOMLEFT:
			if (map_path[i-1].x == next_tile.x + 1) {
				next_tile.y--;
			} else {
				next_tile.x++;
			}
			break;
		case TILE_BOTTOMRIGHT:
			if (map_path[i-1].x == next_tile.x - 1) {
				next_tile.y--;
			} else {
				next_tile.x--;
			}
			break;
		case TILE_NONE:
		default:
			printf("invalid tile found trying to find path\n");
			return 0;
		}

		map_path[i+1] = next_tile;
		map_path_length++;

		// check if we're back to start
		if (map_path[i].x == map_path[0].x && map_path[i].y == map_path[0].y) {
			break;
		}
	}
	printf("path:\n");
	for (int i=0; i<map_path_length; i++) {
		printf("x: %d y: %d\n", map_path[i].x, map_path[i].y);
	}

	return 1;
}

void map_render(SDL_Renderer *ren)
{
	for (unsigned int x=0; x<width; x++) {
		for (unsigned int y=0; y<height; y++) {
			SDL_Rect target;
			target.x = x * TILE_WIDTH;
			target.y = y * TILE_HEIGHT;
			target.w = TILE_WIDTH;
			target.h = TILE_HEIGHT;
			SDL_Texture *texture;
			switch(map_tiles[x][y]) {
			case TILE_HORIZONTAL:
				texture = tile_horizontal;
				break;
			case TILE_VERTICAL:
				texture = tile_vertical;
				break;
			case TILE_UPPERLEFT:
				texture = tile_upperleft;
				break;
			case TILE_UPPERRIGHT:
				texture = tile_upperright;
				break;
			case TILE_BOTTOMLEFT:
				texture = tile_lowerleft;
				break;
			case TILE_BOTTOMRIGHT:
				texture = tile_lowerright;
				break;
			case TILE_NONE:
			default:
				texture = tile_none;
				break;
			}

			SDL_RenderCopy(ren, texture, 0, &target);
		}
	}
	for (int i = 0; i < modifiers_count; i++) {
		SDL_RenderCopy(ren, modifiers[i].texture, 0, &modifiers[i].rect);
	}
}

vec2 map_get_edge_normal(int x, int y)
{
	vec2 normal;
	normal.x = 0;
	normal.y = 0;

	if (x < 0 || y < 0) {
		printf("asked for edge normal for invalid x: %d y: %d\n", x, y);
		return normal;
	}

	const unsigned int px = x / TILE_WIDTH;
	const unsigned int py = y / TILE_HEIGHT;
	if (px >= width || py >= height) {
		printf("asked for edge normal at out of bounds coordinates x: %d y: %d\n", x, y);
		return normal;
	}

	unsigned int rel_x = x - (px * TILE_WIDTH);
	unsigned int rel_y = y - (py * TILE_HEIGHT);

	switch (map_tiles[px][py]) {
	case TILE_HORIZONTAL:
		if (rel_y > TILE_HEIGHT / 2) {
			normal.x = 0;
			normal.y = -1;
		} else {
			normal.x = 0;
			normal.y = 1;
		}
		return normal;
	case TILE_VERTICAL:
		if (rel_x > TILE_WIDTH / 2) {
			normal.x = -1;
			normal.y = 0;
		} else {
			normal.x = 1;
			normal.y = 0;
		}
		return normal;
	case TILE_UPPERLEFT:
		normal.x = (px + 1) * TILE_WIDTH;
		normal.y = (py + 1) * TILE_HEIGHT;
		break;
	case TILE_UPPERRIGHT:
		normal.x = (px) * TILE_WIDTH;
		normal.y = (py + 1) * TILE_HEIGHT;
		break;
	case TILE_BOTTOMLEFT:
		normal.x = (px + 1) * TILE_WIDTH;
		normal.y = (py) * TILE_HEIGHT;
		break;
	case TILE_BOTTOMRIGHT:
		normal.x = (px) * TILE_WIDTH;
		normal.y = (py) * TILE_HEIGHT;
		break;
	case TILE_NONE:
	default:
		break;
	}
	normal.x -= x;
	normal.y -= y;

	return normal;
}

cJSON *map_serialize()
{
	cJSON *map_object = cJSON_CreateObject();
	cJSON_AddNumberToObject(map_object, "tile_width", TILE_WIDTH);
	cJSON_AddNumberToObject(map_object, "tile_height", TILE_HEIGHT);

	cJSON *tile_array = cJSON_CreateArray();
	for (unsigned int y=0; y<height; y++) {
		cJSON *tile_row = cJSON_CreateArray();
		for (unsigned int x=0; x<width; x++) {
			cJSON *tile_item;
			switch(map_tiles[x][y]) {
			case TILE_HORIZONTAL:
				tile_item = cJSON_CreateString("-");
				break;
			case TILE_VERTICAL:
				tile_item = cJSON_CreateString("|");
				break;
			case TILE_UPPERLEFT:
				tile_item = cJSON_CreateString("/");
				break;
			case TILE_UPPERRIGHT:
				tile_item = cJSON_CreateString("`");
				break;
			case TILE_BOTTOMLEFT:
				tile_item = cJSON_CreateString("\\");
				break;
			case TILE_BOTTOMRIGHT:
				tile_item = cJSON_CreateString(",");
				break;
			case TILE_NONE:
			default:
				tile_item = cJSON_CreateString(".");
				break;
			}
			cJSON_AddItemToArray(tile_row, tile_item);
		}
		cJSON_AddItemToArray(tile_array, tile_row);
	}
	cJSON_AddItemToObject(map_object, "tiles", tile_array);

	cJSON *modifiers_array = cJSON_CreateArray();
	for (int i=0; i<modifiers_count; i++) {
		cJSON *modtypestr;
		switch(modifiers[i].type) {
		case MAP_MUD:
			modtypestr = cJSON_CreateString("mud");
			break;
		case MAP_ICE:
			modtypestr = cJSON_CreateString("ice");
			break;
		case MAP_BOOST:
			modtypestr = cJSON_CreateString("booster");
			break;
		default:
			continue;
		}

		cJSON *modifier_object = cJSON_CreateObject();
		cJSON_AddItemToObject(modifier_object, "type", modtypestr);
		cJSON_AddNumberToObject(modifier_object, "x", modifiers[i].rect.x);
		cJSON_AddNumberToObject(modifier_object, "y", modifiers[i].rect.y);
		cJSON_AddNumberToObject(modifier_object, "width", modifiers[i].rect.w);
		cJSON_AddNumberToObject(modifier_object, "height", modifiers[i].rect.h);

		cJSON_AddItemToArray(modifiers_array, modifier_object);
	}
	cJSON_AddItemToObject(map_object, "modifiers", modifiers_array);

	cJSON *path_array = cJSON_CreateArray();
	for (int i=0; i<map_path_length; i++) {
		cJSON *path_item = cJSON_CreateObject();
		cJSON_AddNumberToObject(path_item, "tile_x", map_path[i].x);
		cJSON_AddNumberToObject(path_item, "tile_y", map_path[i].y);
		cJSON_AddItemToArray(path_array, path_item);
	}
	cJSON_AddItemToObject(map_object, "path", path_array);

	return map_object;
}

int map_dist_left_in_tile(int pathcount, vec2 pos)
{
	pathcount++;
	pathcount %= map_path_length;

	const unsigned int next_x = map_path[pathcount].x;
	const unsigned int next_y = map_path[pathcount].y;

	const unsigned int cur_tilex = pos.x / TILE_WIDTH;
	const unsigned int cur_tiley = pos.y / TILE_HEIGHT;

	if (next_x == cur_tilex) {
		if (next_y < cur_tiley) {
			return (pos.y - next_y * TILE_HEIGHT);
		} else {
			return (next_y * TILE_HEIGHT - pos.y);
		}
	} else {
		if (next_x < cur_tilex) {
			return (pos.x - next_x * TILE_WIDTH);
		} else {
			return (next_x * TILE_WIDTH - pos.x);
		}
	}
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
