#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "ColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <fstream>

#define FONT_SIZE 36
#define MARGIN (FONT_SIZE * .5)

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

/*
Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});
*/

PlayMode::PlayMode() : scene(*hexapod_scene) {
	/*
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);
	*/

	// Read from JSON file
	// Referenced:
	// 	 https://github.com/nlohmann/json#examples
	//   Tyler Thompson and Pavan Paravasthu's S20 game4: https://github.com/friskydingo0/15-466-f20-base4/blob/master/TextMode.cpp
	try {
		std::ifstream in("dist/story.json");
		in >> json;
	}
	catch (int e) {
		std::cout << "A JSON exception occurred: #" << e << std::endl;
	}

	//std::string json_txt = json["start"]["text"];
	//std::cout << "json_txt: " << json_txt << std::endl;

	curr_state = "start"; // First state upon beginning

	// The following font handling referenced from:
	//   https://learnopengl.com/In-Practice/Text-Rendering
	//   https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	// Load font
	if (FT_Init_FreeType(&ft_lib)) {
		throw std::runtime_error("Could not init FreeType library");
	}

	// Load face
	if (FT_New_Face(ft_lib, (data_path(font_file)).c_str(), 0, &ft_face)) { // Reference for conversion from string to const char *: https://stackoverflow.com/questions/68339345/how-to-convert-string-to-const-char-in-c
		throw std::runtime_error("Failed to load font");
	}
	
	if (FT_Set_Char_Size(ft_face, FONT_SIZE * 64, FONT_SIZE * 64, 0, 0)) {
		throw std::runtime_error("Failed to set character size");
	}

	// Try loading a glyph to make sure we're good 2 go
	if (FT_Load_Char(ft_face, 'A', FT_LOAD_RENDER)) {
		throw std::runtime_error("Failed to load glyph");
	}

	// Disable byte-alignment restriction (because we're using GL_RED as the texture's
	// internalFormat and format args
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Create hb-ft font
	hb_font = hb_ft_font_create(ft_face, NULL);

	// Create hb buffer
	hb_buffer = hb_buffer_create();

	/*
	std::string sample_txt = "heyyy bestie\nwhat's good, b";
	// TODO: make sure to change last arg (-1) to length of string
	hb_buffer_add_utf8(hb_buffer, sample_txt.c_str(), -1, 0, -1); // Reference for conversion from string to const char *: https://stackoverflow.com/questions/68339345/how-to-convert-string-to-const-char-in-c
	hb_buffer_guess_segment_properties(hb_buffer);
	hb_shape(hb_font, hb_buffer, NULL, 0);
	*/

	// Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Create VBO and VAO for rendering the quads (one glyph per quad)
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

PlayMode::~PlayMode() {
	// Referenced: https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	hb_buffer_destroy(hb_buffer);
	hb_font_destroy(hb_font);

	FT_Done_Face(ft_face);
	FT_Done_FreeType(ft_lib);
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if ((evt.key.keysym.sym == SDLK_1) && !one.pressed) {
			one.downs += 1;
			one.pressed = true;
			return true;
		}
		else if ((evt.key.keysym.sym == SDLK_2) && !two.pressed) {
			two.downs += 1;
			two.pressed = true;
			return true;
		}
		/*
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}*/
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_1) {
			one.pressed = false;
			if (json[curr_state]["choice1"][0] != "none") {
				curr_state = json[curr_state]["choice1"][1];
			}
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_2) {
			two.pressed = false;
			if (json[curr_state]["choice2"][0] != "none") {
				curr_state = json[curr_state]["choice2"][1];
			}
			return true;
		}
		/*
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}*/
	} 
	/*else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}*/

	return false;
}

void PlayMode::update(float elapsed) {

	/*
	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);
	
	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		glm::vec3 at = frame[3];
		Sound::listener.set_position_right(at, right, 1.0f / 60.0f);
	}
	*/

	// Update current state based on what the user chooses (so long as that choice exists)
	//if (one.pressed && (json[curr_state]["choice1"][0] != "none")) {
	//	curr_state = json[curr_state]["choice1"][1];
	//} else if (two.pressed && (json[curr_state]["choice2"][0] != "none")) {
	//	curr_state = json[curr_state]["choice2"][1];
	//}

	//reset button press counters:
	one.downs = 0;
	two.downs = 0;
	//left.downs = 0;
	//right.downs = 0;
	//up.downs = 0;
	//down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	//camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	//glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearColor(0.27f, 0.31f, 0.15f, 1.0f); // Olive background because... yeah <3
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	//scene.draw(*camera);

	/*
	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	*/

	//float x = x_start;
	//float y = y_start;
	float scale = 1.0f;

	// Render text
	// Prompt
	draw_text(json[curr_state]["text"], x_start, y_prompt_start, 1.2f);

	// Choice 1 (if it exists)
	if (json[curr_state]["choice1"][0] != "none") {
		std::string choice_txt = "1. " + (std::string)(json[curr_state]["choice1"][0]);
		draw_text(choice_txt, x_start, y_choice_start, scale);
	}

	// Choice 2 (if it exists)
	if (json[curr_state]["choice2"][0] != "none") {
		std::string choice_txt = "2. " + (std::string)(json[curr_state]["choice2"][0]);
		draw_text(choice_txt, x_start, y_choice_start - (60.0f * scale), scale);
	}

	GL_ERRORS();
}

// Referenced:
//   https://learnopengl.com/In-Practice/Text-Rendering
//   Alyssa Lee and Madeline Anthony's F20 game4: https://github.com/lassyla/game4/blob/master/PlayMode.cpp
void PlayMode::draw_text(std::string text, float x, float y, float scale) {
	//float left_margin = 50.0f;
	//float x = 50.0f;
	//float y = 650.0f;
	//float scale = 1.0f;

	hb_buffer_clear_contents(hb_buffer);
	hb_buffer_add_utf8(hb_buffer, text.c_str(), -1, 0, -1); // Reference for conversion from string to const char *: https://stackoverflow.com/questions/68339345/how-to-convert-string-to-const-char-in-c
	hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR);
	hb_buffer_set_script(hb_buffer, HB_SCRIPT_LATIN);
	hb_buffer_set_language(hb_buffer, hb_language_from_string("en", -1));
	hb_shape(hb_font, hb_buffer, NULL, 0);

	glDisable(GL_DEPTH_TEST);

	// Activate corresponding render state
	glUseProgram(color_texture_program->program);
	glUniform3f(color_texture_program->Color_vec3, text_color.x, text_color.y, text_color.z);
	glm::mat4 projection = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f); // Window dimensions yoinked from main
	glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, &projection[0][0]);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(VAO);

	// Get glyph info from buffer
	unsigned int num_glyphs;
	hb_glyph_info_t *glynfo = hb_buffer_get_glyph_infos(hb_buffer, &num_glyphs);

	// Generate a texture for each glyph we encounter in the buffer, unless it's
	// already been loaded
	for (unsigned int i = 0; i < num_glyphs; i++) {
		FT_UInt cp = (FT_UInt)glynfo[i].codepoint; // TODO: type stuff ??

		// If not in Chars map yet, load this glyph and add to Chars
		if (Chars.count(cp) == 0) {
			//std::cout << "codepoint: " << cp << std::endl;
			// Load char glyph
			if (FT_Load_Glyph(ft_face, cp, FT_LOAD_RENDER)) {
				throw std::runtime_error("Failed to load glyph");
				continue;
			}

			// Generate texture
			unsigned int texture;
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(
				GL_TEXTURE_2D,
				0, GL_RED,
				ft_face->glyph->bitmap.width,
				ft_face->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				ft_face->glyph->bitmap.buffer
			);

			// Set texture options
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Store character for later use
			Character character = {
				texture,
				glm::ivec2(ft_face->glyph->bitmap.width, ft_face->glyph->bitmap.rows),
				glm::ivec2(ft_face->glyph->bitmap_left,  ft_face->glyph->bitmap_top),
				(unsigned int)(ft_face->glyph->advance.x)
			};
			Chars.insert(std::pair<FT_UInt, Character>(cp, character));
		}

		// Update VBO for each character
		Character ch = Chars[cp];

		float xpos = x + ch.Bearing.x * scale;
		float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
		float w = ch.Size.x * scale;
		float h = ch.Size.y * scale;

		float vertices[6][4] = {
			{ xpos,     ypos + h, 0.0f, 0.0f },
			{ xpos,     ypos,     0.0f, 1.0f },
			{ xpos + w, ypos,     1.0f, 1.0f },

			{ xpos,     ypos + h, 0.0f, 0.0f },
			{ xpos + w, ypos,     1.0f, 1.0f },
			{ xpos + w, ypos + h, 1.0f, 0.0f }
		};

		// Render glyph texture over quad
		glBindTexture(GL_TEXTURE_2D, ch.TextureID);

		// Update content of VBO memory
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Render quad
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Advance cursors for next glyph (advance is number of 1/64 pixels)
		x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64)

		// New line if this is a space, comma, or period and your cup overfloweth,
		// OR if it's just a newline char lol
		if ((x > 400.0f && ((cp == 32) || (cp == 44) || (cp == 46))) ||
			(cp == 0)) {
			y -= 50.0f * scale;
			x = x_start;
		}
	}

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

/*
glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}
*/
