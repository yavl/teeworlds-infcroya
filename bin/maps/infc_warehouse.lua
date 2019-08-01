-- shrinking from 6500 to 800 (with shrink speed = 1) takes 114 seconds
-- why: 6500 - 800 = 5700; 5700 / TickSpeed = 114; where TickSpeed = 50
default_radius = 6500
min_radius = 800
circle_shrink_speed = 1
default_inf_radius = 360
timelimit = 3

circle_positions = {}

inf_circle_positions = {
	-- x, y, radius
	37, 31, default_inf_radius,
	9, 31, 250,
}

math.randomseed(os.time())

function infc_init()
	-- infc_init() is called 10 secs after round start
	-- hardcode passed C/C++ values here (e.g for tests)
	-- passed values: infc_num_players
	case = math.random(1, 3)
	if case == 1 then
		circle_positions = {
			60, 9, default_radius, min_radius, circle_shrink_speed,
		}
	end
	
	if case == 2 then
		circle_positions = {
			106, 17, default_radius, min_radius, circle_shrink_speed,
		}
	end

	if case == 3 then
		circle_positions = {
			149, 17, default_radius, min_radius, circle_shrink_speed,
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
