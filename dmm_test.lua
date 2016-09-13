require "dmm"
require "utils"

mm = dmm("/dev/ttyUSB1")
while true do
	local temp, time = mm:read()
	if temp then
		print(temp)
		sleep(0.1)
	end
end
