package org.ourtaiko.fanmade;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.*;

/** Bounded, cancellable transfers; never publishes an unverified download. */
final class UpdateFiles {
    interface Progress { void update(long received, long total); }

    static void interrupted() throws InterruptedIOException {
        if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Update cancelled");
    }

    static String hash(Path file) throws IOException {
        MessageDigest digest = sha256();
        try (InputStream input = Files.newInputStream(file)) {
            byte[] buffer = new byte[65536];
            int n;
            while ((n = input.read(buffer)) != -1) {
                interrupted();
                digest.update(buffer, 0, n);
            }
        }
        return hex(digest.digest());
    }

    static MessageDigest sha256() {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException impossible) { throw new AssertionError(impossible); }
    }

    static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder();
        for (byte b : bytes) result.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
        return result.toString();
    }

    static HttpURLConnection connect(URL url) throws IOException {
        for (int redirects = 0; redirects <= 5; redirects++) {
            interrupted();
            if (!url.getProtocol().equals("https") || url.getUserInfo() != null)
                throw new IOException("Updates require HTTPS");
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setInstanceFollowRedirects(false);
            connection.setConnectTimeout(8000);
            connection.setReadTimeout(8000);
            connection.setRequestProperty("User-Agent", "OurTaiko-Android-Updater");
            connection.setRequestProperty("Accept-Encoding", "identity");
            int status;
            try { status = connection.getResponseCode(); }
            catch (IOException error) { connection.disconnect(); throw error; }
            if (status == 200) return connection;
            String location = connection.getHeaderField("Location");
            connection.disconnect();
            if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
                    && location != null) url = new URL(url, location);
            else throw new IOException("Update HTTP " + status);
        }
        throw new IOException("Too many update redirects");
    }

    static String metadata(URL url) throws IOException {
        HttpURLConnection connection = connect(url);
        try (InputStream input = connection.getInputStream(); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[4096];
            int n;
            while ((n = input.read(buffer)) != -1) {
                interrupted();
                if (output.size() + n > 16384) throw new IOException("Update metadata too large");
                output.write(buffer, 0, n);
            }
            return output.toString(StandardCharsets.UTF_8.name());
        } finally { connection.disconnect(); }
    }

    static void receive(InputStream input, Path target, String expectedHash, long expectedSize,
                        Progress progress) throws IOException {
        if (!expectedHash.matches("[0-9a-f]{64}") || expectedSize <= 0 || expectedSize > 4L * 1024 * 1024 * 1024)
            throw new IOException("Invalid update metadata");
        Files.createDirectories(target.getParent());
        Path partial = target.resolveSibling(target.getFileName() + ".part");
        try {
            MessageDigest digest = sha256();
            long received = 0;
            try (OutputStream output = Files.newOutputStream(partial)) {
                byte[] buffer = new byte[65536];
                int n;
                while ((n = input.read(buffer)) != -1) {
                    interrupted();
                    received += n;
                    if (received > expectedSize) throw new IOException("Update exceeds declared size");
                    digest.update(buffer, 0, n);
                    output.write(buffer, 0, n);
                    progress.update(received, expectedSize);
                }
            }
            if (received != expectedSize || !hex(digest.digest()).equals(expectedHash))
                throw new IOException("Update checksum/size mismatch");
            interrupted();
            Files.move(partial, target, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
        } finally { Files.deleteIfExists(partial); }
    }
}
