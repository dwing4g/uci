-- for LuaJIT 2.0-beta6 or later

local ffi = require("ffi")
ffi.cdef[[
	int  __stdcall UCIDecode(const void* src, int srclen, void** dst, int* stride, int* width, int* height, int* bit);
	void __stdcall UCIFree(void* p);
	void __stdcall UCIDebug(int level);
]]
local ucidec = ffi.load("ucidec")

if not ucidec then
	print "ERROR: can not load ucidec.dll"
	return -1
end

local fsrc = io.open("test.uci", "rb")
if not fsrc then
	print "ERROR: can not open test.uci"
	return -2
end

local src = fsrc:read("*a")
fsrc:close()
if not src then
	print "ERROR: can not read test.uci"
	return -3
end

local dst = ffi.new("void*[1]")
local stride = ffi.new("int[1]")
local w = ffi.new("int[1]")
local h = ffi.new("int[1]")
local b = ffi.new("int[1]")

ucidec.UCIDebug(0x7fffffff)
local r = ucidec.UCIDecode(src, #src, dst, stride, w, h, b)
if r < 0 then
	print(string.format("ERROR: DecodeUCI failed (return %d)", r))
	return -4
end
print(string.format("INFO: width x height x bit : %d x %d x %d", w[0], h[0], b[0]))

local fdst = io.open("test.rgb", "wb")
if not fdst then
	print "ERROR: can not create test.rgb"
	return -5
end

local dstbase = ffi.cast("char*", dst[0])
local linesize = w[0] * (b[0] / 8)
for i = 1, h[0] do
	fdst:write(ffi.string(dstbase + i * stride[0], linesize))
end
fdst:close()

ucidec.UCIFree(dst[0])

return 0
