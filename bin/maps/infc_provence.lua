-- shrinking from 6500 to 800 (with shrink speed = 1) takes 114 seconds
-- why: 6500 - 800 = 5700; 5700 / TickSpeed = 114; where TickSpeed = 50
default_radius = 6500
min_radius = 800
circle_shrink_speed = 1
default_inf_radius = 300
timelimit = 3

circle_positions = {}

inf_circle_positions = {
	-- x, y, radius
	101, 118, default_inf_radius,
	136, 117, default_inf_radius,
	164, 116, default_inf_radius,
	183, 115, default_inf_radius,
}

math.randomseed(os.time())

function infc_init()
	-- infc_init() is called 10 secs after round start
	-- hardcode passed C/C++ values here (e.g for tests)
	-- passed values: infc_num_players
	case = math.random(1, 15) -- 15 different cases
	if case == 1 then
		circle_positions = { 
			-- x, y, radius, minradius
			63, 58, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 2 then
		circle_positions = { 
			-- x, y, radius, minradius
			62, 84, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 3 then
		circle_positions = { 
			-- x, y, radius, minradius
			26, 57, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 4 then
		circle_positions = { 
			-- x, y, radius, minradius
			35, 86, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 5 then
		circle_positions = { 
			-- x, y, radius, minradius
			66, 123, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 6 then
		circle_positions = { 
			-- x, y, radius, minradius
			225, 133, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 7 then
		circle_positions = { 
			-- x, y, radius, minradius
			232, 86, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 8 then
		circle_positions = { 
			-- x, y, radius, minradius
			226, 29, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 9 then
		circle_positions = { 
			-- x, y, radius, minradius
			198, 41, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 10 then
		circle_positions = { 
			-- x, y, radius, minradius
			147, 73, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 11 then
		circle_positions = { 
			-- x, y, radius, minradius
			120, 61, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 12 then
		circle_positions = { 
			-- x, y, radius, minradius
			222, 19, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 13 then
		circle_positions = { 
			-- x, y, radius, minradius
			199, 22, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 14 then
		circle_positions = { 
			-- x, y, radius, minradius
			69, 36, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 15 then
		circle_positions = { 
			-- x, y, radius, minradius
			45, 45, default_radius, min_radius, circle_shrink_speed,
		}
	end
end

-- Try not to modify functions below
function infc_get_circle_pos()
	return circle_positions
end

function infc_get_inf_circle_pos()
	return inf_circle_positions
end

function infc_get_timelimit()
	return timelimit
end

function infc_get_circle_min_radius()
	return min_radius
end

function infc_get_circle_shrink_speed()
	return circle_shrink_speed
end