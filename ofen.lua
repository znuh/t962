
require "utils"
require "t962"

function run_fan()
	while true do
		oven:update(false, true)
		sleep(0.1)
	end
end

function update_stats(t_start, tm, t_soll, t_pcb, t_air)
	local logstr = string.format("%.1f %.1f %.1f %.1f",tm-t_start,t_soll,t_pcb,t_air)
	--print(logstr)
	if log then
		log:write(logstr.."\n")
		log:flush()
	end
	if gnuplot and (last_gnuplot == nil) or (tm - last_gnuplot >= 1) then
		local gpstr = string.format("%.1f %.1f %.1f\n",t_soll,t_pcb,t_air)
		gnuplot:write(gpstr)
		gnuplot:flush()
		last_gnuplot = tm
	end
	
	-- time above 217°C
	if stats.start_217 and t_pcb < 217 and not stats.end_217 then
		stats.end_217 = tm-t_start
		stats.above_217 = stats.end_217 - stats.start_217
		print("t > 217°C:",stats.above_217,"seconds");
		print("   t_peak:",stats.t_peak,"°C") 
	elseif not stats.start_217 and t_pcb >= 217 then
		stats.start_217 = tm-t_start
	end
	
	-- peak temperature
	if not stats.t_peak then
		stats.t_peak = t_pcb
	else
		stats.t_peak = math.max(stats.t_peak, t_pcb)
	end
	
end

function oven_loop(oven, wave)
	
	local function get_tsoll(elapsed)
		local wv_idx = math.floor(elapsed/10) + 1
		local t_soll = nil
		
		-- waves with chart array
		if wave.chart then
			local t_next = wave.chart[wv_idx + 1]
			if t_next == nil then return end
			local delta = t_next - wave.chart[wv_idx]
			local time_ofs = elapsed % 10
			delta = (delta * time_ofs) / 10
			t_soll = wave.chart[wv_idx] + delta
			
		-- waves with ramp up rate, peak temp + hold time, ramp down rate
		else
			local ramp_down = math.abs(wave.ramp_down)
			local r_up_end = (wave.peak_temp - 25) / wave.ramp_up
			local peak_end = r_up_end + wave.peak_hold
			local r_down_end = ((wave.peak_temp - 25) / ramp_down) + peak_end
			
			if elapsed <= r_up_end then
				t_soll = 25 + (elapsed * wave.ramp_up)
			elseif elapsed <= peak_end then
				t_soll = wave.peak_temp
			elseif elapsed <= r_down_end then
				t_soll = wave.peak_temp - ((elapsed - peak_end) * ramp_down)
			end
		end
		
		return t_soll
	end
	
	local function get_tist(t1,t2)
		-- conversion function from original T962 firmware
		local t_air = (t1 + t2 + 40) / 2
		-- polynomial for estimation of PCB temperature
		local t_pcb = t_air + (0.18*t_air)
		return t_pcb, t_air
	end
	
	local function orig_fw_controller(t_soll, t_ist)
		local lamp_threshold = 2
		-- original firmware threshold: 5
		-- local lamp_threshold = 5
		local fan_threshold = -10
		
		local lamp_ctl = ((t_soll - t_ist) >= lamp_threshold)
		local fan_ctl = ((t_soll - t_ist) <= fan_threshold)
		
		return lamp_ctl, fan_ctl
	end
	
	stats = {}
	
	local lamp, fan = false, false
	local last_update = nil
	-- set controller algorithm
	local temp_controller = orig_fw_controller
	local delay = 0.1
	local t_start = gettime()
	
	-- logfile
	if log then
		log:close()
		log = nil
	end
	local logname = math.floor(t_start) .. ".txt"
	log = io.open(logname,"w")
	
	-- gnuplot
	if gnuplot then
		gnuplot:close()
		gnuplot = nil
	end
	if use_gnuplot then
		gnuplot = io.popen("feedgnuplot --stream 0.1 --lines --xmin 0 --ymax 900 --ymin 20 --ymax 270 --set 'ytics 20' --legend 0 'soll' --legend 1 'PCB' --legend 2 'Luft'","w")
	end
	
	local last_lamp = 0
	
	while true do
		local elapsed = math.floor(gettime() - t_start)
		local t_soll = get_tsoll(elapsed)
		
		-- leave loop when done
		if t_soll == nil then break end
		
		local t1, t2, tm = oven:update(lamp, fan)
		local t_pcb, t_air = get_tist(t1, t2)
		
		-- use multimeter sensor if available
		if mm then
			local t_mm, mm_tm = mm:read()
			t_pcb = t_mm
		end
		
		-- limit to 1 Hz
		if elapsed ~= last_update then
			local t_ist = t_pcb
			lamp, fan = temp_controller(t_soll, t_ist)
			if last_lamp >= 2 then
				lamp = false
			end
			t1, t2, tm = oven:update(lamp, fan)
			last_update = elapsed
			if lamp then
				last_lamp = last_lamp + 1
			else
				last_lamp = 0
			end
		end
		
		update_stats(t_start, tm, t_soll, t_pcb, t_air)
		
		sleep(delay)
		
	end
	
	log:close()
	log=nil
	
	-- done
end
