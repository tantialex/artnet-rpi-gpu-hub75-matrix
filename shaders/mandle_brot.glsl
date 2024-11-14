
void mainImage(out vec4 O, vec2 U) {
  vec2 R = iResolution.xy, z;
  U = ( U+U - R ) / R.y;
  U.x -= .5;
  for( O *= 0. ; O.r < 1. && dot(z,z)<6.; O += .02 )
    z = mat2( z, -z.y, z ) * z + U;
}
