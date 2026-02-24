extends Node3D

@onready var mi := $MeshInstance3D

func _ready():
	# vértices do triângulo (em espaço de objeto)
	var vertices := PackedVector3Array([
		Vector3(-0.5, -0.5, 0.0), # V0
		Vector3( 0.5, -0.5, 0.0), # V1
		Vector3( 0.0,  0.5, 0.0)  # V2
	])

	# cores por vértice: R, G, B
	var colors := PackedColorArray([
		Color(1,0,0,1),  # V0 = vermelho
		Color(0,1,0,1),  # V1 = verde
		Color(0,0,1,1)   # V2 = azul
	])

	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_COLOR]  = colors

	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)

	mi.mesh = mesh
	
func _process(delta):
	# Escala pulsante entre 0.5 e 1.5
	var s = 1.0 + sin(Time.get_ticks_msec() / 1000.0) * 0.5
	mi.scale = Vector3(s, s, s)
