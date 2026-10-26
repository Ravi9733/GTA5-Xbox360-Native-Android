package com.gta5.nativeandroid;

import android.content.ContentResolver;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import java.io.Closeable;
import java.io.IOException;

/**
 * Seekable, read-only access to a document selected through Android's Storage Access Framework.
 *
 * This deliberately exposes offsets rather than pretending a SAF Uri is a POSIX path.
 * SanRecomp's filesystem adapter can build on this API, but this class alone does not mount
 * archives or execute game code.
 */
public final class SafRandomAccessFile implements Closeable {
    private final ParcelFileDescriptor descriptor;
    private boolean closed;

    private SafRandomAccessFile(ParcelFileDescriptor descriptor) {
        this.descriptor = descriptor;
    }

    public static SafRandomAccessFile open(ContentResolver resolver, Uri uri) throws IOException {
        if (resolver == null || uri == null) {
            throw new IllegalArgumentException("resolver and uri are required");
        }
        ParcelFileDescriptor descriptor = resolver.openFileDescriptor(uri, "r");
        if (descriptor == null) {
            throw new IOException("Document provider did not return a file descriptor");
        }
        return new SafRandomAccessFile(descriptor);
    }

    public synchronized long length() throws IOException {
        ensureOpen();
        try {
            return Os.fstat(descriptor.getFileDescriptor()).st_size;
        } catch (ErrnoException e) {
            throw new IOException("Unable to query document length", e);
        }
    }

    /**
     * Reads at an absolute offset without changing a shared file cursor.
     *
     * @return bytes read, or 0 when offset is at/past EOF
     */
    public synchronized int readAt(long offset, byte[] destination, int destinationOffset,
                                   int requestedLength) throws IOException {
        ensureOpen();
        if (offset < 0 || destination == null || destinationOffset < 0 || requestedLength < 0
                || destinationOffset > destination.length - requestedLength) {
            throw new IllegalArgumentException("Invalid read range");
        }
        if (requestedLength == 0) return 0;

        int total = 0;
        while (total < requestedLength) {
            try {
                int count = Os.pread(descriptor.getFileDescriptor(), destination,
                        destinationOffset + total, requestedLength - total, offset + total);
                if (count <= 0) break;
                total += count;
            } catch (ErrnoException e) {
                if (e.errno == OsConstants.EINTR) continue;
                throw new IOException("Random-access document read failed", e);
            }
        }
        return total;
    }

    private void ensureOpen() throws IOException {
        if (closed) throw new IOException("Document is closed");
    }

    @Override
    public synchronized void close() throws IOException {
        if (closed) return;
        closed = true;
        descriptor.close();
    }
}
