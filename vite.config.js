import { defineConfig } from "vite";

export default defineConfig({
  build: {
    outDir: "dist-viewer",
    rollupOptions: {
      input: "viewer/index.html",
    },
  },
});
