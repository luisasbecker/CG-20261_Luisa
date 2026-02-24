shader_type spatial;
render_mode unshaded,cull_disabled;

void vertex() {
	// Called for every vertex the material is visible on.
	POSITION = PROJECTION_MATRIX * VIEW_MATRIX * MODEL_MATRIX  * vec4(VERTEX, 1.0);

}



void fragment() {
    // COLOR vem do atributo de vértice (Mesh.ARRAY_COLOR)
    ALBEDO = COLOR.rgb;
    ALPHA  = COLOR.a;
}


//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
