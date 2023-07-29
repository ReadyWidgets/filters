# Surface filters for AwesomeWM

## Description

This repository adds support for various pixel effects, such as blurring or recoloring, that can be used by simply wrapping a widget in them.

## Usage

First, clone this repository to your awesome wm config directory:

```bash
git clone https://github.com/ReadyWidgets/filters "${XDG_CONFIG_HOME:-$HOME/.config}/awesome/filters"
```

Next, compile `c-filters.c`:

```bash
cd "${XDG_CONFIG_HOME:-$HOME/.config}/awesome/filters"
bash build.sh
```

Then, you can use the filters using something like this:

```lua
local mywidget = wibox.widget {
	{
		markup = "<b>Hello,</b> <i>world!</i>",
		halign = "center",
		valign = "center",
		widget = wibox.widget.textbox,
	},
	radius  = 20,
	opacity = 1.0,
	widget  = filters.shadow,
}
```

## Available filters and properties

- Blur
	- `radius: integer = 10`
	- `no_use_kernel: boolean = false`
- Shadow
	- `radius: integer = 10`
	- `opacity: number = 1.0`
- Tint
	- `color: string = "#000000"`
