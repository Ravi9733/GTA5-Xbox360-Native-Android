package com.gta5.nativeandroid;

import android.content.ContentResolver;
import android.net.Uri;

import java.io.Closeable;
import java.io.IOException;

/**
 * Seekable native bridge for a user-selected game document.
 *
 * The document remains owned by Android's Storage Access Framework; this wrapper does not
 * turn a content URI into a filesystem path or imply that RPF/XEX parsing is implemented.
 */
public final class NativeGameFile implements Closeable {
    private final MainActivity owner;
    private long handle;

    private NativeGameFile(MainActivity owner, long handle) {
        this.owner = owner;
        this.handle = handle;
    }

    public static NativeGameFile open(MainActivity owner, Uri documentUri) throws IOException {
        if (owner == null || documentUri == null) {
            throw new IllegalArgumentException("owner and documentUri are required");
        }
        ContentResolver resolver = owner.getContentResolver();
        long handle = owner.nativeOpenDocument(resolver, documentUri);
        if (handle == 0) {
            throw new IOException("Native SAF open failed");
        }
        return new NativeGameFile(owner, handle);
    }

    public synchronized long length() throws IOException {
        ensureOpen();
        long length = owner.nativeDocumentLength(handle);
        if (length < 0) throw new IOException("Native SAF length query failed");
        return length;
    }

    /** Read up to requestedLength bytes at an absolute offset. Returns an empty array at EOF. */
    public synchronized byte[] readAt(long offset, int requestedLength) throws IOException {
        ensureOpen();
        if (offset < 0 || requestedLength < 0 || requestedLength > 16 * 1024 * 1024) {
            throw new IllegalArgumentException("Invalid native read range");
        }
        byte[] bytes = owner.nativeReadDocumentAt(handle, offset, requestedLength);
        if (bytes == null) throw new IOException("Native SAF read failed");
        return bytes;
    }

    private void ensureOpen() throws IOException {
        if (handle == 0) throw new IOException("Native game file is closed");
    }

    @Override
    public synchronized void close() {
        if (handle == 0) return;
        long closing = handle;
        handle = 0;
        owner.nativeCloseDocument(closing);
    }
}
