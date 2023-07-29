---@meta c-filters

---@class filters.c-filters
local m = {}

---@param pixbuf userdata A native Pixbuf
---@param radius integer Blur radius in pixels
---@param no_use_kernel boolean If true, no kernel will be used; instead the pixels will simply get averaged
function m.blur_pixbuf(pixbuf, radius, no_use_kernel) end

---@param pixbuf userdata A native Pixbuf
---@param radius integer Shadow radius in pixels
---@param opacity number Shadow opacity, from 0.0 to 1.0
function m.add_shadow_to_pixbuf(pixbuf, radius, opacity) end

---@param pixbuf userdata A native Pixbuf
---@param red number
---@param green number
---@param blue number
function m.tint_pixbuf(pixbuf, red, green, blue) end
