package com.gta5.nativeandroid;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Read-only, relative-path view of an extracted game folder selected with ACTION_OPEN_DOCUMENT_TREE.
 *
 * This is a storage adapter primitive for a future native runtime bridge. It does not mount RPF
 * archives, parse XEX files, or execute the game.
 */
public final class SafGameFileSystem {
    private final ContentResolver resolver;
    private final Uri treeUri;
    private final String rootDocumentId;

    public SafGameFileSystem(ContentResolver resolver, Uri treeUri) {
        if (resolver == null || treeUri == null) {
            throw new IllegalArgumentException("resolver and treeUri are required");
        }
        this.resolver = resolver;
        this.treeUri = treeUri;
        this.rootDocumentId = DocumentsContract.getTreeDocumentId(treeUri);
    }

    /** Resolve a slash-separated path relative to the selected folder, case-insensitively. */
    public Uri resolve(String relativePath) throws IOException {
        if (relativePath == null) throw new IllegalArgumentException("relativePath is required");
        String normalized = relativePath.replace('\\', '/');
        while (normalized.startsWith("/")) normalized = normalized.substring(1);
        if (normalized.isEmpty()) {
            return DocumentsContract.buildDocumentUriUsingTree(treeUri, rootDocumentId);
        }

        String[] parts = normalized.split("/");
        String parentId = rootDocumentId;
        for (int i = 0; i < parts.length; i++) {
            String part = parts[i];
            if (part.isEmpty() || ".".equals(part) || "..".equals(part)) {
                throw new IOException("Invalid relative game path");
            }
            Uri match = findChild(parentId, part);
            if (match == null) return null;
            if (i < parts.length - 1) {
                if (!DocumentsContract.isTreeUri(treeUri)) {
                    throw new IOException("Selected URI is not a document tree");
                }
                String id = DocumentsContract.getDocumentId(match);
                if (!isDirectory(id)) throw new IOException("Path component is not a directory: " + part);
                parentId = id;
            } else {
                return match;
            }
        }
        return null;
    }

    /** Opens an existing file for seekable, read-only access; returns null if not found. */
    public SafRandomAccessFile openFile(String relativePath) throws IOException {
        Uri uri = resolve(relativePath);
        if (uri == null) return null;
        if (isDirectory(DocumentsContract.getDocumentId(uri))) {
            throw new IOException("Requested path is a directory: " + relativePath);
        }
        return SafRandomAccessFile.open(resolver, uri);
    }

    /** Lists immediate child names. Empty means an empty directory; null means path missing. */
    public List<String> list(String relativeDirectory) throws IOException {
        Uri directory = resolve(relativeDirectory);
        if (directory == null) return null;
        String documentId = DocumentsContract.getDocumentId(directory);
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, documentId);
        List<String> names = new ArrayList<>();
        try (Cursor cursor = resolver.query(childrenUri,
                new String[]{DocumentsContract.Document.COLUMN_DISPLAY_NAME}, null, null, null)) {
            if (cursor == null) throw new IOException("Document provider returned no directory cursor");
            while (cursor.moveToNext()) {
                String name = cursor.getString(0);
                if (name != null) names.add(name);
            }
        } catch (RuntimeException e) {
            throw new IOException("Unable to list game directory: " + relativeDirectory, e);
        }
        return names;
    }

    private Uri findChild(String parentId, String wantedName) throws IOException {
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentId);
        try (Cursor cursor = resolver.query(childrenUri,
                new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME},
                null, null, null)) {
            if (cursor == null) throw new IOException("Document provider returned no directory cursor");
            while (cursor.moveToNext()) {
                String name = cursor.getString(1);
                if (name != null && name.toLowerCase(Locale.ROOT)
                        .equals(wantedName.toLowerCase(Locale.ROOT))) {
                    return DocumentsContract.buildDocumentUriUsingTree(treeUri, cursor.getString(0));
                }
            }
        } catch (RuntimeException e) {
            throw new IOException("Unable to resolve game path component: " + wantedName, e);
        }
        return null;
    }

    private boolean isDirectory(String documentId) throws IOException {
        Uri documentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, documentId);
        try (Cursor cursor = resolver.query(documentUri,
                new String[]{DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null)) {
            if (cursor == null || !cursor.moveToFirst()) {
                throw new IOException("Unable to inspect document type");
            }
            return DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(0));
        } catch (RuntimeException e) {
            throw new IOException("Unable to inspect document type", e);
        }
    }
}
