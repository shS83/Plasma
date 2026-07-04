import colorsys
import math
import sys
import random
import numpy as np
import pygame as pg
""" CONCEIVED AND DIRECTED BY PROMETHEUS
CODE FORGED WITH THE CHATGPT GREMLIN """

class PlasmaEffect:
	def __init__(self, width=320, height=200):
		self.width = width
		self.height = height

		self.time = 0.0
		self.speed = 1.0
		self.palette_speed = 180.0
		self.intensity = 1.6
		self.surface = pg.Surface((width, height))

		# pygame.surfarray käyttää järjestystä width × height.
		x = np.arange(width, dtype=np.float32)[:, None]
		y = np.arange(height, dtype=np.float32)[None, :]

		self.x = x
		self.y = y

		center_x = width * 0.5
		center_y = height * 0.5

		self.distance = np.sqrt(
			(x - center_x) ** 2 +
			(y - center_y) ** 2
		)

		self.palette_hold_time = 5.0
		self.palette_transition_time = 2.0

		self.palettes = [
			self.create_duotone_palette(
				(0, 0, 255),
				(20, 20, 50),
			),
			self.create_duotone_palette(
				(255, 0, 0),
				(50, 20, 20),
			),
			self.create_duotone_palette(
				(255, 255, 20),
				(50, 50, 20),
			),
			self.create_duotone_palette(
				(15, 0, 90),
				(255, 0, 180),
			),
			self.create_duotone_palette(
				(0, 30, 120),
				(0, 255, 210),
			),
			self.create_duotone_palette(
				(120, 0, 20),
				(255, 180, 0),
			),
			self.create_duotone_palette(
				(20, 100, 0),
				(180, 255, 20),
			),
			self.create_grayscale_palette(levels=16),
			self.create_rainbow_palette(levels=32),
			self.create_rainbow_palette(levels=255),
		]

		self.palette = random.choice(self.palettes)
		pg.font.init()

		self.scroller_font = pg.font.Font(
			"scroller.ttf",
			26,
		)

		self.scroller_text = (
			"    OLD SCHOOL PLASMA AND RAINBOW SCROLLER"
			"  MADE TO HONOR THE OLD BEARDS OF THE SCENE  "
			"      YOU ARE KINGS             "
			"  HAILS TO FAIRLIGHT :: FARBRAUSCH :: NEURO ::  "
			"RAZOR1911 :: SPACEBALLS :: ANDROMEDA SOFTWARE DEVELOPMENT :: "
			"KEWLERZ       JUST TO MENTION A FEW ... YOU KNOW WHO YOU ARE     "
			"KUDOS TO THE OG CODERS AND TRACKER MUSICIANS - "
			"STAY TUNED FOR MORE PSYCHEDELIC VISIONS   "
			"CONCEIVED, DIRECTED AND MUSIC BY PROMETHEUS -- CODE FORGED WITH THE CHATGPT GREMLIN ***   "
		)

		# Arvot ovat sisäisen 320 × 200 -resoluution pikseleitä.
		self.scroller_base_y = 100
		self.scroller_amplitude = 6.0
		self.scroller_wavelength = 100.0
		self.scroller_wave_speed = 1.0
		self.scroller_speed = 100.0
		self.scroller_letter_spacing = 1
		self.scroller_gap = 0
		self.scroller_rainbow_speed = 0.8
		self.scroller_rainbow_density = 1.8
		self.scroller_rainbow_brightness = 1.0
		self.scroller_rainbow_saturation = 1.0
		self.scroller_rainbow_wavelength = 300.0
		self.scroller_rainbow_speed = 0.18
		self.scroller_rainbow_levels = 32
		self.scroller_outline_thickness = 2
		self.scroller_outline_alpha = 220

		self.scroller_glyphs, self.scroller_width = (
			self.create_scroller_glyphs(self.scroller_text)
		)

	def create_grayscale_palette(self, levels=16):
		palette = np.zeros((256, 3), dtype=np.uint8)

		for i in range(256):
			phase = i / 255.0
			value = 0.5 - 0.5 * math.cos(phase * math.tau)
			value = int(value * 255)

			if levels > 1:
				step = 255 / (levels - 1)
				value = int(round(value / step) * step)

			palette[i] = (
				value,
				value,
				value,
			)

		return palette

	def create_duotone_palette(self, color_a, color_b):
		palette = np.zeros((256, 3), dtype=np.uint8)

		for i in range(256):
			phase = i / 255.0

			# Väri A -> väri B -> väri A.
			mix = 0.5 - 0.5 * math.cos(phase * math.tau)

			for channel in range(3):
				value = (
						color_a[channel] * (1.0 - mix) +
						color_b[channel] * mix
				)

				palette[i, channel] = int(value)

		return palette

	def create_rainbow_palette(self, levels=256):
		palette = np.zeros((256, 3), dtype=np.uint8)

		for i in range(256):
			bucket = int(i * levels / 256) % levels
			hue = bucket / levels

			r, g, b = colorsys.hsv_to_rgb(
				hue,
				1.0,  # saturation
				1.0,  # brightness
			)

			palette[i] = (
				int(r * 255),
				int(g * 255),
				int(b * 255),
			)

		return palette

	def get_current_palette(self):
		cycle_length = (
				self.palette_hold_time +
				self.palette_transition_time
		)

		cycle_index = int(self.time // cycle_length)

		current_index = cycle_index % len(self.palettes)
		next_index = (current_index + 1) % len(self.palettes)

		cycle_time = self.time % cycle_length

		if cycle_time < self.palette_hold_time:
			return self.palettes[current_index]

		blend = (
						cycle_time - self.palette_hold_time
				) / self.palette_transition_time

		# Pehmeä smoothstep-siirtymä.
		blend = blend * blend * (3.0 - 2.0 * blend)

		current_palette = self.palettes[current_index].astype(np.float32)
		next_palette = self.palettes[next_index].astype(np.float32)

		palette = (
				current_palette * (1.0 - blend) +
				next_palette * blend
		)

		return palette.astype(np.uint8)

	def create_scroller_glyphs(self, text):
		glyphs = []
		total_width = 0

		for character in text:
			mask, outline, shadow, width = self.create_glyph(
				character,
			)

			glyphs.append(
				(
					mask,
					outline,
					shadow,
					width,
				)
			)

			total_width += width + self.scroller_letter_spacing

		return glyphs, total_width

	def draw_outline(self, target, outline, x, y, thickness):
		for oy in range(-thickness, thickness + 1):
			for ox in range(-thickness, thickness + 1):
				if ox == 0 and oy == 0:
					continue

				# Vähän pyöreämpi reuna kuin pelkkä neliö.
				if ox * ox + oy * oy <= thickness * thickness + 0.5:
					target.blit(
						outline,
						(x + ox, y + oy),
					)

	def create_glyph(self, character):
		if character == " ":
			width = max(
				1,
				self.scroller_font.size(" ")[0],
			)
			return None, None, None, width

		mask = self.scroller_font.render(
			character,
			True,
			(255, 255, 255),
		).convert_alpha()

		width, height = mask.get_size()

		if width <= 0 or height <= 0:
			return None, None, None, 1

		mask_alpha = pg.surfarray.array_alpha(mask)

		# Musta outline-surface
		outline = pg.Surface(
			(width, height),
			pg.SRCALPHA,
			32,
		).convert_alpha()

		outline_alpha = pg.surfarray.pixels_alpha(outline)
		outline_alpha[:] = (
				mask_alpha.astype(np.uint16) * self.scroller_outline_alpha // 255
		).astype(np.uint8)
		del outline_alpha

		# Musta hieman läpinäkyvä varjo
		shadow = pg.Surface(
			(width, height),
			pg.SRCALPHA,
			32,
		).convert_alpha()

		shadow_alpha = pg.surfarray.pixels_alpha(shadow)
		shadow_alpha[:] = (
				mask_alpha.astype(np.uint16) * 180 // 255
		).astype(np.uint8)
		del shadow_alpha

		return mask, outline, shadow, width

	def create_rainbow_text_surface(self, mask, ribbon_x):
		width, height = mask.get_size()

		rainbow = pg.Surface(
			(width, height),
			pg.SRCALPHA,
			32,
		).convert_alpha()

		mask_alpha = pg.surfarray.array_alpha(mask)

		for local_x in range(width):
			global_x = ribbon_x + local_x

			hue = (
					      global_x / self.scroller_rainbow_wavelength +
					      self.time * self.scroller_rainbow_speed
			      ) % 1.0

			# Hieman porrastettu oldschool-sateenkaari.
			levels = self.scroller_rainbow_levels
			hue = int(hue * levels) / levels

			# Kevyt helmeilevä kirkkausvaihtelu.
			brightness = (
					0.78 +
					0.22 * math.sin(
				global_x / 18.0 -
				self.time * 3.0
			)
			)

			brightness = max(
				0.0,
				min(1.0, brightness),
			)

			r, g, b = colorsys.hsv_to_rgb(
				hue,
				1.0,
				brightness,
			)

			color = (
				int(r * 255),
				int(g * 255),
				int(b * 255),
				255,
			)

			pg.draw.line(
				rainbow,
				color,
				(local_x, 0),
				(local_x, height - 1),
			)

		rainbow_alpha = pg.surfarray.pixels_alpha(rainbow)
		rainbow_alpha[:] = mask_alpha
		del rainbow_alpha

		return rainbow

	def draw_scroller_copy(self, target, start_x):
		cursor_x = float(start_x)
		ribbon_x = 0.0

		for mask, outline, shadow, glyph_width in self.scroller_glyphs:
			center_x = cursor_x + glyph_width * 0.5

			phase = (
					center_x / self.scroller_wavelength * math.tau +
					self.time * self.scroller_wave_speed
			)

			center_y = (
					self.scroller_base_y +
					math.sin(phase) * self.scroller_amplitude
			)

			if mask is not None:
				x = int(cursor_x)
				y = int(center_y - mask.get_height() * 0.5)

				if x + glyph_width >= 0 and x < target.get_width():
					rainbow = self.create_rainbow_text_surface(
						mask,
						ribbon_x,
					)

					# 1) musta reunus
					self.draw_outline(
						target,
						outline,
						x,
						y,
						self.scroller_outline_thickness,
					)

					# 2) varjo
					target.blit(
						shadow,
						(x + 1, y + 2),
					)

					# 3) varsinainen värillinen glyph
					target.blit(
						rainbow,
						(x, y),
					)

			advance = glyph_width + self.scroller_letter_spacing
			cursor_x += advance
			ribbon_x += advance

	def draw_scroller(self, target):
		cycle_width = (
				self.scroller_width +
				self.scroller_gap
		)

		distance = (
				self.time *
				self.scroller_speed
		)

		# Ensimmäinen teksti alkaa ruudun oikean reunan ulkopuolelta.
		first_x = self.width - distance

		# Kun teksti on kokonaan poistunut vasemmalta,
		# siirretään se seuraavalle kierrokselle.
		while first_x + cycle_width < 0:
			first_x += cycle_width

		x = first_x

		while x < self.width:
			self.draw_scroller_copy(
				target,
				x,
			)

			x += cycle_width

	def start(self):
		self.time = 0.0

	def update(self, dt, audio=None):
		self.time += dt * self.speed

		# Myöhemmin tähän voidaan syöttää musiikin kick-arvo 0.0–1.0.
		if audio is not None:
			kick = getattr(audio, "kick", 0.0)
			self.intensity = 1.0 + kick * 0.35
		else:
			self.intensity += (1.0 - self.intensity) * min(1.0, dt * 4.0)

	def render(self, target):
		t = self.time

		plasma = (
				np.sin(self.x * 0.045 + t * 1.30) +
				np.sin(self.y * 0.060 - t * 1.10) +
				np.sin((self.x + self.y) * 0.035 + t * 0.75) +
				np.sin(self.distance * 0.080 - t * 1.65)
		)

		plasma *= self.intensity

		indexes = ((plasma + 4.0) * 31.875).astype(np.int16)

		palette_offset = int(t * self.palette_speed) % 256
		indexes = ((indexes.astype(np.int32) + palette_offset) % 256).astype(np.uint8)

		palette = self.get_current_palette()
		rgb = palette[indexes]

		pg.surfarray.blit_array(
			self.surface,
			rgb,
		)

		# Teksti piirretään matalaresoluutioiseen plasmapintaan.
		self.draw_scroller(self.surface)

		scaled = pg.transform.scale(
			self.surface,
			target.get_size(),
		)

		target.blit(
			scaled,
			(0, 0),
		)

	def stop(self):
		pass


def main():
	pg.init()
	pg.mixer.init()
	pg.mixer.music.load("./doomscroller.ogg")
	pg.mixer.music.play(-1)
	screen = pg.display.set_mode(
		(1280, 800),
		pg.RESIZABLE,
	)

	pg.display.set_caption("Retro Plasma")

	clock = pg.time.Clock()
	plasma = PlasmaEffect(320, 200)
	plasma.start()

	running = True

	while running:
		dt = clock.tick(60) / 1000.0

		for event in pg.event.get():
			if event.type == pg.QUIT:
				running = False

			elif event.type == pg.KEYDOWN:
				if event.key == pg.K_ESCAPE:
					running = False

				elif event.key == pg.K_UP:
					plasma.speed += 0.1

				elif event.key == pg.K_DOWN:
					plasma.speed = max(0.1, plasma.speed - 0.1)

				elif event.key == pg.K_RIGHT:
					plasma.palette_speed += 5.0

				elif event.key == pg.K_LEFT:
					plasma.palette_speed -= 5.0

		plasma.update(dt)
		plasma.render(screen)

		pg.display.flip()

	pg.quit()
	sys.exit()


if __name__ == "__main__":
	main()
