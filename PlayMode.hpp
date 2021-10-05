#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include "json.hpp"
using JSON = nlohmann::json;

#include <glm/glm.hpp>

#include <hb.h>
#include <hb-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <vector>
#include <deque>
#include <map>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	/*
	//hexapod leg to wobble:
	Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;

	glm::vec3 get_leg_tip_position();

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	*/

	//camera:
	Scene::Camera *camera = nullptr;

	// Referenced: https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	hb_font_t *hb_font;
	hb_buffer_t *hb_buffer;

	// Referenced: https://learnopengl.com/In-Practice/Text-Rendering
	FT_Library ft_lib;  // FreeType library into which we will load our font
	FT_Face    ft_face; // Loading font into ft as a face
	std::string font_file = "VT323-Regular.ttf";

	struct Character {
		unsigned int TextureID; // ID handle of glyph texture
		glm::ivec2   Size;      // Size of glyph
		glm::ivec2   Bearing;   // Offset from baseline to top left of glyph
		unsigned int Advance;   // Offset to advance to next glyph
	};

	std::map< FT_UInt, Character > Chars;

	GLuint VBO, VAO; // Vertex buffer object & vertex array object

	// RGB text color, each value in range of 0 to 1
	glm::vec3 text_color = glm::vec3(179.0f/256.0f, 207.0f/256.0f, 120.0f/256.0f);

	void draw_text(std::string text, float x, float y, float scale);

	float x_start = 50.0f;
	float y_prompt_start = 650.0f;
	float y_choice_start = 300.0f;

	JSON json;
	std::string curr_state;
};
