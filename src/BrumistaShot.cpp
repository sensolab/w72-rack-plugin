// Brumista's screenshots. The rendering follows Rack's Window::screenshotModules (Rack,
// GPL-3.0-or-later, by VCV): a FramebufferWidget draws offscreen and glReadPixels reads it back.
// PNG encoding is stb_image_write (public domain, by Sean Barrett), compiled static so it
// can't clash with the older copy inside Rack.
#include "BrumistaShot.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "dep/stb_image_write.h"
#pragma GCC diagnostic pop

namespace brumista {

using namespace rack;

namespace {

// Draws through a function, so a live widget can be drawn where it stands in the scene.
struct Canvas : widget::Widget {
	std::function<void(const DrawArgs&)> paint;
	void draw(const DrawArgs& args) override {
		paint(args);
	}
};

void appendBytes(void* context, void* data, int size) {
	static_cast<std::string*>(context)->append(static_cast<const char*>(data), size);
}

// Draws paint over size Rack pixels into a framebuffer, zoom pixels per Rack pixel, and
// encodes the result. Runs between frames, where nothing else is drawing.
Shot render(math::Vec size, float zoom, std::function<void(const widget::Widget::DrawArgs&)> paint) {
	GLint maxTexture = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
	float side = std::ceil(std::max(size.x, size.y) * zoom) + 1;
	if (side > maxTexture)
		throw std::runtime_error(string::f("the GPU can't draw an image %d px wide", (int) side));

	widget::FramebufferWidget fbw;
	// Oversampling smooths fine panel detail, as in Rack's -t; it doubles each side first.
	fbw.oversample = (side * 2 <= maxTexture) ? 2 : 1;
	fbw.box.size = size;
	Canvas* canvas = new Canvas;
	canvas->box.size = size;
	canvas->paint = paint;
	fbw.addChild(canvas);
	// The framebuffer is sized for the screen's pixel ratio, which the scale undoes.
	float pixelRatio = std::fmax(1.f, std::floor(APP->window->pixelRatio));
	fbw.render(math::Vec(zoom, zoom).div(pixelRatio));

	NVGLUframebuffer* fb = fbw.getFramebuffer();
	if (!fb)
		throw std::runtime_error("could not create a framebuffer");
	int width = 0, height = 0;
	nvgImageSize(APP->window->vg, fbw.getImageHandle(), &width, &height);
	std::vector<uint8_t> pixels((size_t) width * height * 4);
	nvgluBindFramebuffer(fb);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	nvgluBindFramebuffer(NULL);

	Shot shot;
	shot.width = width;
	shot.height = height;
	// OpenGL rows run bottom up.
	stbi_flip_vertically_on_write(1);
	if (!stbi_write_png_to_func(appendBytes, &shot.png, width, height, 4, pixels.data(), width * 4))
		throw std::runtime_error("could not encode the PNG");
	return shot;
}

} // namespace

Shot shotModule(app::ModuleWidget* mw, float zoom) {
	return render(mw->box.size, zoom, [mw](const widget::Widget::DrawArgs& args) {
		widget::Widget::DrawArgs a = args;
		a.clipBox = mw->box.zeroPos();
		mw->draw(a);
		// Lights, and displays drawn on the light layer.
		mw->drawLayer(a, 1);
	});
}

Shot shotRack(math::Rect region, float zoom) {
	return render(region.size, zoom, [region](const widget::Widget::DrawArgs& args) {
		widget::Widget::DrawArgs a = args;
		a.clipBox = region;
		nvgSave(args.vg);
		nvgTranslate(args.vg, -region.pos.x, -region.pos.y);
		APP->scene->rack->draw(a);
		nvgRestore(args.vg);
	});
}

std::string base64(const std::string& data) {
	static const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve((data.size() + 2) / 3 * 4);
	size_t i = 0;
	for (; i + 2 < data.size(); i += 3) {
		uint32_t n = (uint8_t) data[i] << 16 | (uint8_t) data[i + 1] << 8 | (uint8_t) data[i + 2];
		out += ALPHABET[n >> 18 & 63];
		out += ALPHABET[n >> 12 & 63];
		out += ALPHABET[n >> 6 & 63];
		out += ALPHABET[n & 63];
	}
	if (i < data.size()) {
		bool two = i + 1 < data.size();
		uint32_t n = (uint8_t) data[i] << 16 | (two ? (uint8_t) data[i + 1] << 8 : 0);
		out += ALPHABET[n >> 18 & 63];
		out += ALPHABET[n >> 12 & 63];
		out += two ? ALPHABET[n >> 6 & 63] : '=';
		out += '=';
	}
	return out;
}

} // namespace brumista
