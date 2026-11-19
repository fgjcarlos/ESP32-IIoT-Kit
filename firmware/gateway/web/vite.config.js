// Stable output names for the flash-served portal (issue #69, W02).
//
// The firmware embeds the committed dist/ files, so every build must
// produce the same file set: no content hashing. Relative base so the
// portal works when served from the gateway root.
import { defineConfig } from 'vite';

export default defineConfig({
  base: './',
  build: {
    rollupOptions: {
      output: {
        entryFileNames: 'app.js',
        chunkFileNames: 'chunks/[name].js',
        assetFileNames: 'assets/[name].[ext]',
      },
    },
  },
});
