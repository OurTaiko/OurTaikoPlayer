package org.ourtaiko.fanmade;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Install the skins shipped with this APK before SDL starts, never a moving remote branch. */
final class BundledSkinUpdater {
    static final String MARKER = ".bundled-skins-version";

    static void install(GameDataInstaller.Assets assets, Path root) throws IOException {
        install(assets, root, false);
    }

    static void install(GameDataInstaller.Assets assets, Path root, boolean freshlyInstalled) throws IOException {
        root = root.toAbsolutePath().normalize();
        String revision;
        Map<String, Integer> expected = new LinkedHashMap<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(assets.open("GameData.skins"), StandardCharsets.UTF_8))) {
            revision = reader.readLine();
            if (revision == null || !revision.matches("[0-9a-f]{64}")) throw new IOException("Invalid skin revision");
            String line;
            while ((line = reader.readLine()) != null) {
                String[] parts = line.split(" ");
                if (parts.length != 2 || !parts[0].matches("[a-zA-Z0-9_-]+")) throw new IOException("Invalid skin name");
                int count;
                try { count = Integer.parseInt(parts[1]); }
                catch (NumberFormatException error) { throw new IOException("Invalid skin count", error); }
                if (count <= 0 || expected.put(parts[0], count) != null) throw new IOException("Invalid skin count");
            }
        }
        if (expected.isEmpty()) throw new IOException("No bundled skins");
        Path marker = root.resolve(MARKER);
        if (Files.isRegularFile(marker) && Files.size(marker) == 64
                && new String(Files.readAllBytes(marker), StandardCharsets.UTF_8).equals(revision)) return;
        Files.createDirectories(root);
        // The initial installer already validated and copied the same APK archive.
        if (freshlyInstalled) {
            Path temporary = Files.createTempFile(root, ".skin-version-", ".tmp");
            try {
                Files.write(temporary, revision.getBytes(StandardCharsets.UTF_8));
                Files.move(temporary, marker, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
            } finally { Files.deleteIfExists(temporary); }
            return;
        }
        Path stage = Files.createTempDirectory(root, ".skin-update-");
        try {
            Map<String, Integer> counts = new HashMap<>();
            Set<Path> seen = new HashSet<>();
            byte[] buffer = new byte[65536];
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(assets.open("GameData.zip")))) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    UpdateFiles.interrupted();
                    if (entry.getName().startsWith("Skins/") && !entry.isDirectory()) {
                        String[] parts = entry.getName().split("/", 3);
                        if (parts.length != 3 || !expected.containsKey(parts[1])) throw new IOException("Unexpected bundled skin");
                        Path skin = stage.resolve(parts[1]);
                        Path file = skin.resolve(parts[2]).normalize();
                        if (!file.startsWith(skin) || file.equals(skin) || !seen.add(file)) throw new IOException("Invalid skin path");
                        Files.createDirectories(file.getParent());
                        try (OutputStream output = Files.newOutputStream(file, StandardOpenOption.CREATE_NEW)) {
                            int n;
                            while ((n = zip.read(buffer)) != -1) {
                                UpdateFiles.interrupted();
                                output.write(buffer, 0, n);
                            }
                        }
                        counts.merge(parts[1], 1, Integer::sum);
                    }
                    zip.closeEntry();
                }
            }
            if (!counts.equals(expected)) throw new IOException("Incomplete skin archive");
            for (String skin : expected.keySet())
                if (!Files.isRegularFile(stage.resolve(skin).resolve("Graphics/skin_config.json")))
                    throw new IOException("Missing skin config");
            Path skins = root.resolve("Skins"), backups = root.resolve(".skin-backups");
            Files.createDirectories(skins);
            Files.createDirectories(backups);
            if (Files.isSymbolicLink(skins) || Files.isSymbolicLink(backups)) throw new IOException("Invalid skin directory");
            Path backup = Files.createTempDirectory(backups, revision.substring(0, 12) + "-");
            for (String name : expected.keySet()) {
                UpdateFiles.interrupted();
                Path target = skins.resolve(name);
                if (Files.exists(target, LinkOption.NOFOLLOW_LINKS)) Files.move(target, backup.resolve(name));
                try { Files.move(stage.resolve(name), target); }
                catch (IOException failure) {
                    if (Files.exists(backup.resolve(name), LinkOption.NOFOLLOW_LINKS)) Files.move(backup.resolve(name), target);
                    throw failure;
                }
            }
            Path tempMarker = stage.resolve("revision");
            Files.write(tempMarker, revision.getBytes(StandardCharsets.UTF_8));
            Files.move(tempMarker, marker, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
        } finally {
            // Only this invocation's fresh staging directory is removed; backups remain intact.
            try (java.util.stream.Stream<Path> paths = Files.walk(stage)) {
                for (Path path : (Iterable<Path>) paths.sorted(Comparator.reverseOrder())::iterator) Files.deleteIfExists(path);
            }
        }
    }
}
