package org.ourtaiko.fanmade;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

public class UpdateFilesTest {
    static void check(boolean value) { if (!value) throw new AssertionError(); }
    interface Action { void run() throws Exception; }
    static void fails(Action action) throws Exception {
        try { action.run(); throw new AssertionError("Expected failure"); }
        catch (IOException expected) { }
    }
    static byte[] archive(Map<String, String> entries) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(output)) {
            for (Map.Entry<String, String> entry : entries.entrySet()) {
                zip.putNextEntry(new ZipEntry(entry.getKey()));
                zip.write(entry.getValue().getBytes(java.nio.charset.StandardCharsets.UTF_8));
                zip.closeEntry();
            }
        }
        return output.toByteArray();
    }
    static GameDataInstaller.Assets assets(byte[] zip, int count) {
        String revision = UpdateFiles.hex(UpdateFiles.sha256().digest(zip));
        return name -> new ByteArrayInputStream(name.equals("GameData.zip") ? zip
                : (revision + "\nPyTaikoGreen " + count + "\n").getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }
    public static void main(String[] args) throws Exception {
        Path root = Files.createTempDirectory("ourtaiko-updater-test-");
        try {
            byte[] data = "verified apk".getBytes();
            String hash = UpdateFiles.hex(UpdateFiles.sha256().digest(data));
            Path apk = root.resolve("update.apk");
            UpdateFiles.receive(new ByteArrayInputStream(data), apk, hash, data.length, (n,t) -> {});
            check(UpdateFiles.hash(apk).equals(hash));
            fails(() -> UpdateFiles.receive(new ByteArrayInputStream(data), apk, "0".repeat(64), data.length, (n,t) -> {}));
            fails(() -> UpdateFiles.receive(new ByteArrayInputStream(data), apk, hash, data.length-1, (n,t) -> {}));
            fails(() -> UpdateFiles.receive(new ByteArrayInputStream(data), apk, hash, data.length+1, (n,t) -> {}));
            fails(() -> UpdateFiles.receive(new ByteArrayInputStream(data), apk, hash, data.length, (n,t) -> Thread.currentThread().interrupt()));
            Thread.interrupted();
            check(UpdateFiles.hash(apk).equals(hash));
            check(!Files.exists(root.resolve("update.apk.part")));
            fails(() -> UpdateFiles.connect(new java.net.URL("http://example.com/update")));
            Path game = root.resolve("game");
            Files.createDirectories(game.resolve("Skins/Custom"));
            Files.writeString(game.resolve("Skins/Custom/custom.lua"), "custom");
            Files.writeString(game.resolve("scores.db"), "scores");
            byte[] first = archive(Map.of("Skins/PyTaikoGreen/Graphics/skin_config.json", "{}", "Skins/PyTaikoGreen/Fonts/font.ttf", "font"));
            Path fresh = root.resolve("fresh");
            GameDataInstaller.Assets initial = name -> name.equals("GameData.count")
                    ? new ByteArrayInputStream("2".getBytes()) : assets(first, 2).open(name);
            GameDataInstaller.install(initial, fresh);
            BundledSkinUpdater.install(name -> {
                if (name.equals("GameData.zip")) throw new AssertionError("fresh install copied skins twice");
                return initial.open(name);
            }, fresh, true);
            check(!Files.exists(fresh.resolve(".skin-backups")));
            BundledSkinUpdater.install(assets(first, 2), game);
            Files.writeString(game.resolve("Skins/PyTaikoGreen/local.lua"), "my edit");
            BundledSkinUpdater.install(name -> {
                if (name.equals("GameData.zip")) throw new AssertionError("repeat launch scanned ZIP");
                return assets(first, 2).open(name);
            }, game);
            byte[] next = archive(Map.of("Skins/PyTaikoGreen/Graphics/skin_config.json", "new", "Skins/PyTaikoGreen/Fonts/new.ttf", "new font"));
            BundledSkinUpdater.install(assets(next, 2), game);
            check(Files.readString(game.resolve("Skins/PyTaikoGreen/Graphics/skin_config.json")).equals("new"));
            check(!Files.exists(game.resolve("Skins/PyTaikoGreen/local.lua")));
            try (java.util.stream.Stream<Path> paths = Files.walk(game.resolve(".skin-backups"))) {
                check(paths.anyMatch(p -> p.getFileName().toString().equals("local.lua")));
            }
            check(Files.readString(game.resolve("Skins/Custom/custom.lua")).equals("custom"));
            check(Files.readString(game.resolve("scores.db")).equals("scores"));
            byte[] unsafe = archive(Map.of("Skins/PyTaikoGreen/../../escape", "bad"));
            fails(() -> BundledSkinUpdater.install(assets(unsafe, 1), game));
            fails(() -> BundledSkinUpdater.install(assets(first, 3), game));
            check(Files.readString(game.resolve("Skins/PyTaikoGreen/Graphics/skin_config.json")).equals("new"));
            Thread.currentThread().interrupt();
            fails(() -> BundledSkinUpdater.install(assets(first, 2), game));
            Thread.interrupted();
            BundledSkinUpdater.install(assets(first, 2), game);
            check(Files.exists(game.resolve("Skins/PyTaikoGreen/Fonts/font.ttf")));
            System.out.println("Update transfer and bundled skin upgrade tests passed");
        } finally {
            Thread.interrupted();
            try (java.util.stream.Stream<Path> paths = Files.walk(root)) {
                for (Path path : (Iterable<Path>) paths.sorted(Comparator.reverseOrder())::iterator) Files.delete(path);
            }
        }
    }
}
