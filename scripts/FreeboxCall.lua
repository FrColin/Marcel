-- FreeboxCall
-- 
-- This function publish Freebox call
--
function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

function FreeboxCall( section_name )
	print("*F* FreeboxCall ...")
	--local bugy = Marcel.CallFreeboxApi(1)
	local calls = Marcel.CallFreeboxApi('call/log')
	if calls == nil then
		print("Error no result from CallFreeboxApi\n")
		return
	end
	for callCount = 1, #calls do
		local call = calls[callCount];
		if call.new == true then
			print("*F* FreeboxCall reply ",dump (call))
		end
	end
	

	local topic = 'Telephone/'

	--Marcel.MQTTPublish( topic , f)
	
end

