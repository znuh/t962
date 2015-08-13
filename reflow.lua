require "t962"
require "utils"
require "dmm"

use_gnuplot = 1

if oven == nil then
	oven = t962("/dev/ttyUSB0")
end

function dmm_init()
	mm = dmm("/dev/ttyUSB1")
end

dofile("ofen.lua")

xilinx_wv = {
	ramp_up = (250-25)/300, -- 5 min ramp, < 1°C/s
	peak_temp = 250,
	peak_hold = 10,
	ramp_down = -1, -- -1°C/s
}

temper = {
	ramp_up = 0.75,
	peak_temp = 70,
	peak_hold = 600,
	ramp_down = -1,
}

function run_wave(wv)
	oven_loop(oven, wv)
	run_fan()
end
