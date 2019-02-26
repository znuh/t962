require "t962"
require "utils"

use_gnuplot = 1

if oven == nil then
	oven = t962("/dev/ttyUSB3")
end

dofile("ofen.lua")

lmpa_q6_wv = {
	ramp_up = 1, -- 1°C/s
	peak_temp = 200,
	peak_hold = 30,
	ramp_down = -1, -- -1°C/s
}

xilinx_wv = {
	ramp_up = (250-25)/300, -- 5 min ramp, < 1°C/s
	peak_temp = 250,
	peak_hold = 10,
	ramp_down = -1, -- -1°C/s
}

pb_wv = {
	ramp_up = (245-25)/300, -- 5 min ramp, < 1°C/s
	peak_temp = 245,
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
