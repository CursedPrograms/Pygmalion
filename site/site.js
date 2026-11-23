// Pygmalion's page: the feature cards and the year
(function () {
  'use strict';
  const features = [
    ['MakeHuman, tab for tab', 'Files, Modelling, Geometries, Materials, Pose/Animate, Rendering, Settings, Utilities: the same body, the same 1,280 shapes, the same .mhm files both ways.'],
    ['Sliders for everything', 'Gender, age, muscle, weight, height, proportions, ethnicity, and every face and body slider with its picture; the view turns to what you move.'],
    ['Dress them', 'Clothes, hair, eyebrows, eyelashes, teeth, eyes and skins; download MakeHuman\'s 60+ community asset packs from inside the program.'],
    ['Real strand hair', 'Blender\'s hair tools (Generate, Clump, Curl, Frizz, Noise, Trim...) with all their sliders, TressFX physics that falls, swings and follows poses, and TressFX export for game engines.'],
    ['Skin that glows', 'Subsurface scattering: light spreads under the skin and shines through ears and fingers lit from behind.'],
    ['Pose them', 'Four skeletons, 18 poses, 33 expressions and a mixer for every face pose unit; animations and motion capture play on the body.'],
    ['Take them anywhere', 'Collada, FBX, glTF, OBJ, Ogre3D, STL and BVH, with the skeleton, its weights and the textures, posed or not, in any units.'],
    ['Render, script, mass produce', 'Pictures up to 8K with a transparent background; a script for every job; hundreds of random people saved and exported at once.']
  ];
  const grid = document.getElementById('features');
  if (grid) {
    for (const [title, text] of features) {
      const card = document.createElement('article');
      const h = document.createElement('h2');
      h.textContent = title;
      const p = document.createElement('p');
      p.textContent = text;
      card.append(h, p);
      grid.appendChild(card);
    }
  }
  const year = document.getElementById('year');
  if (year) year.textContent = String(Math.max(2026, new Date().getFullYear()));
})();
