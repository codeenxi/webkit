if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify that that cursors weakly hold request, and work if request is GC'd");

indexedDBTest(prepareDatabase, onOpen);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    store.put("value1", "key1");
    store.put("value2", "key2");
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store')");
    evalAndLog("store = tx.objectStore('store')");

    evalAndLog("cursorRequest = store.openCursor()");
    cursorRequest.onsuccess = function openCursorRequestSuccess(evt) {
        preamble(evt);
        debug("Result will be checked later, to ensure that lazy access is safe");
    };

    evalAndLog("otherRequest = store.get(0)");
    otherRequest.onsuccess = function otherRequestSuccess(evt) {
        preamble(evt);

        debug("Verify that the request's result can be accessed lazily:");
        evalAndLog("gc()");

        evalAndLog("cursor = cursorRequest.result");
        shouldBeNonNull("cursor");
        shouldBeEqualToString("cursor.key", "key1");
        shouldBeEqualToString("cursor.value", "value1");
        evalAndLog("cursorRequest.extra = 123");
        evalAndLog("cursor.extra = 456");

        // Assign a new handler to inspect the request and cursor indirectly.
        cursorRequest.onsuccess = function cursorContinueSuccess(evt) {
            preamble(evt);
            evalAndLog("cursor = event.target.result");
            shouldBeNonNull("cursor");
            shouldBeEqualToString("cursor.key", "key2");
            shouldBeEqualToString("cursor.value", "value2");
            shouldBe("event.target.extra", "123");
            shouldBe("cursor.extra", "456");
        };

        debug("Ensure request is not released if cursor is still around.");
        cursorRequestObservation = internals.observeGC(cursorRequest);
        evalAndLog("cursorRequest = null");
        evalAndLog("gc()");
        shouldBeFalse("cursorRequestObservation.wasCollected");

        evalAndLog("cursor.continue()");

        cursorObservation = internals.observeGC(cursor);
        evalAndLog("cursor = null");
        evalAndLog("gc()");
        shouldBeFalse("cursorObservation.wasCollected");
        shouldBeFalse("cursorRequestObservation.wasCollected");

        evalAndLog("finalRequest = store.get(0)");
        finalRequest.onsuccess = function finalRequestSuccess(evt) {
            preamble(evt);
            shouldBeEqualToString("cursor.key", "key2");
            shouldBeEqualToString("cursor.value", "value2");

            cursorObservation = internals.observeGC(cursor);
            evalAndLog("cursor = null");
            evalAndLog("gc()");
            shouldBeTrue("cursorRequestObservation.wasCollected");
            shouldBeTrue("cursorObservation.wasCollected");
        };
    };

    tx.oncomplete = finishJSTest;
}
