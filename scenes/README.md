We provide several example scenes to demonstrate different spectral behavior we can control.

- The [Bunny](./bunny.json) scene demonstrates simple metamerism under multiple illuminants. Toggle between the left light sources (FL11, D65) and one of the scene objects will become identical to the other.
- The [Cornell Box](./cornell_box.json) scene demonstrates editing of indirect light. Modify the provided spectral constraint to see that the indirect red scattering on the left-most cornell box depends very much on the box' spectral reflectance.
- The [Fold](./fold.json) scene demonstrates editing of interreflection colors. The entire scene comprises one reflectance, and yet the single provided constraint can let you add a significant color component to the folded region.
- The [Mug](./mug.json) scene demonstrates metamerism of textured surfaces under close-to-direct light. Toggle between the FL2 and D65 light sources and the mug's texture appearance will change drastically.
- The [Portal](./portal.json) scene demonstrates editing of indirect-light color behavior. Almost the entire scene is indirectly illuminated by a far-off light, shining through a portal.

We also provide several scenes used in the paper.

- The [Patches](./paper_patches.json) scene provides the setup used in Fig. 4. Two over-exposed patches are shown, of identical input color, but uplifted to different metamers s.t. their interreflections mismatch.
- The [Indirect color constraining](./paper_indirect_color_constraining.json) scene provides the setup used in Fig. 8. A single patch is shown, entirely of one input color, but uplifted to a metamer s.t. the folded part changes color.