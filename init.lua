local awful = require("awful")
local gears = require("gears")
local wibox = require("wibox")

local lgi = require("lgi")
local Gdk, GdkPixbuf, cairo = lgi.Gdk, lgi.GdkPixbuf, lgi.cairo

---@type filters.c-filters
local c_filters = require("filters.c-filters")

local assert_param_type, fill_context_with_surface, gen_property, copy

local export = {}

--- Check if a given parameter is of the expected type, otherwise throw an error
---@param func_name string
---@param position integer
---@param wanted_type type
---@param value any
function assert_param_type(func_name, position, wanted_type, value)
	local value_type = type(value)

	assert(value_type == wanted_type, ("Wrong type of parameter #%d passed to '%s' (expected %s, got %s)"):format(position, func_name, wanted_type, value_type))
end

--- Convert a cairo surface into a Gdk pixel buffer
---@param surface cairo.Surface
---@param width integer
---@param height integer
---@return GdkPixbuf.Pixbuf
function export.surface_to_pixbuf(surface, width, height)
	return Gdk.pixbuf_get_from_surface(surface, 0, 0, width, height)
end

---@param pixbuf GdkPixbuf.Pixbuf
---@return cairo.Surface
function export.pixbuf_to_surface(pixbuf)
	local surface = awesome.pixbuf_to_surface(pixbuf._native, gears.surface())

	if cairo.Surface:is_type_of(surface) then
		return surface
	end

	return cairo.Surface(surface, true)
end

--- Fill a cairo context with a cairo surface
---@param cr cairo.Context
---@param surface cairo.Surface
function fill_context_with_surface(cr, surface)
	cr:set_source_surface(surface, 0, 0)
	cr:paint()
end

function gen_property(object, property)
	assert_param_type("gen_property", 1, "table",  object)
	assert_param_type("gen_property", 2, "string", property)

	object["get_" .. property] = function(self)
		--print("Called get_"..property.."()")
		return self._private[property]
	end

	object["set_" .. property] = function(self, value)
		--print("Called set_"..property.."(" .. tostring(value) .. ")")
		self._private[property] = value
		self:emit_signal(("property::" .. property), value)
		self._private.force_redraw = true
		self:emit_signal("widget::redraw_needed")
	end
end

do
	local base = { mt = {} }
	base.mt.__index = base.mt
	setmetatable(base, base.mt)

	function base:get_widget()
		return self._private.widget
	end

	function base:set_widget(widget)
		if self._private.child_redraw_listener == nil then
			function self._private.child_redraw_listener()
				self._private.force_redraw = true
				self:emit_signal("widget::redraw_needed")
			end
		end

		local child_redraw_listener = self._private.child_redraw_listener

		if self._private.widget ~= nil then
			self._private.widget:disconnect_signal("widget::redraw_needed", child_redraw_listener)
		end

		widget:connect_signal("widget::redraw_needed", child_redraw_listener)

		wibox.widget.base.set_widget_common(self, widget)
	end

	function base:get_children()
		return { self._private.widget }
	end

	function base:set_children(children)
		self:set_widget(children[1])
	end

	function base:draw(context, cr, w, h)
		--do
		--	return fill_context_with_surface(cr, self._private.cached_surface)
		--end

		local child = self:get_widget()

		if child == nil then
			return
		end

		if (not self._private.force_redraw) and (self._private.cached_surface ~= nil) then
			fill_context_with_surface(cr, self._private.cached_surface)
			return
		end

		---@type cairo.Surface
		local surface
		if self.on_draw ~= nil then
			surface = self:on_draw(cr, w, h, child)
		else
			surface = wibox.widget.draw_to_image_surface(child, w, h)
		end

		self._private.force_redraw = false
		self._private.cached_surface = surface

		fill_context_with_surface(cr, surface)
	end

	function base.mt.__call(cls, kwargs)
		if kwargs == nil then
			kwargs = {}
		end

		local self = gears.object { enable_properties = true }

		gears.table.crush(self, wibox.widget.base.make_widget())
		gears.table.crush(self, cls)

		if cls.parse_kwargs ~= nil then
			cls.parse_kwargs(kwargs)
		end

		if self._private == nil then
			self._private = {}
		end

		--gears.table.crush(self, kwargs)
		for k, v in pairs(kwargs) do
			self[k] = v
		end

		return self
	end

	export.base = base
end

---@generic T1 : table
---@param tb T1
---@return T1
function copy(tb)
	local copy_of_tb = {}

	for k, v in pairs(tb) do
		copy_of_tb[k] = v
	end

	setmetatable(copy_of_tb, getmetatable(tb))

	return copy_of_tb
end

do
	local blur = copy(export.base)

	gen_property(blur, "radius")
	gen_property(blur, "no_use_kernel")

	function blur:on_draw(cr, w, h, child)
		local pixbuf = export.surface_to_pixbuf(wibox.widget.draw_to_image_surface(child, w, h), w, h)._native

		local blurred_pixbuf = GdkPixbuf.Pixbuf(c_filters.blur_pixbuf(pixbuf, self.radius or 10, self.no_use_kernel or false))

		return fill_context_with_surface(cr, export.pixbuf_to_surface(blurred_pixbuf))
	end

	function blur.parse_kwargs(kwargs)
		kwargs.radius = kwargs.radius or 10
		kwargs.no_use_kernel = kwargs.no_use_kernel or false
	end

	export.blur = blur
end

do
	local shadow = copy(export.base)

	gen_property(shadow, "radius")
	gen_property(shadow, "opacity")

	function shadow:on_draw(cr, w, h, child)
		local pixbuf = export.surface_to_pixbuf(wibox.widget.draw_to_image_surface(child, w, h), w, h)._native

		local blurred_pixbuf = GdkPixbuf.Pixbuf(c_filters.add_shadow_to_pixbuf(pixbuf, self.radius or 10, self.opacity or 1.0))

		return fill_context_with_surface(cr, export.pixbuf_to_surface(blurred_pixbuf))
	end

	function shadow.parse_kwargs(kwargs)
		kwargs.radius = kwargs.radius or 10
		kwargs.opacity = kwargs.opacity or 1.0
	end

	export.shadow = shadow
end

do
	local tint = copy(export.base)

	gen_property(tint, "color")

	local floor = math.floor

	function tint:on_draw(cr, w, h, child)
		local pixbuf = export.surface_to_pixbuf(wibox.widget.draw_to_image_surface(child, w, h), w, h)._native

		local r, g, b, _ = gears.color.parse_color(self.color or "#000000")
		r = floor(r * 255)
		g = floor(g * 255)
		b = floor(b * 255)
		local blurred_pixbuf = GdkPixbuf.Pixbuf(c_filters.tint_pixbuf(pixbuf, r, g, b))

		return fill_context_with_surface(cr, export.pixbuf_to_surface(blurred_pixbuf))
	end

	function tint.parse_kwargs(kwargs)
		kwargs.color = kwargs.color or "#000000"
	end

	export.tint = tint
end

return export
