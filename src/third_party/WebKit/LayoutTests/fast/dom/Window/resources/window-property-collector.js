function collectProperties()
{
    // Collect properties of the top-level window, since touching the properties
    // of a DOMWindow affects its internal C++ state.
    collectPropertiesHelper(window, []);

    propertiesToVerify.sort(function (a, b)
    {
        if (a.property < b.property)
            return -1
        if (a.property > b.property)
            return 1;
        return 0;
    });
}

function emitExpectedResult(path, expected)
{
    // Skip internals properties, since they aren't web accessible.
    if (path[0] == 'internals'
        || path[0] == 'propertiesToVerify' // Skip the list we're building...
        || path[0] == 'clientInformation' // Just an alias for navigator.
        || path[0] == 'testRunner' // Skip testRunner since they are only for testing.
        || path[0] == 'layoutTestController' // Just an alias for testRunner.
        || path[0] == 'eventSender') { // Skip eventSender since they are only for testing.
        return;
    }

    // Skip the properties which are hard to expect a stable result.
    if (path[0] == 'accessibilityController' // we can hardly estimate the states of the cached WebAXObjects.
        || path[0] == 'localStorage') { // local storage is not reliably cleared between tests.
        return;
    }

    // FIXME: Skip MemoryInfo for now, since it's not implemented as a DOMWindowProperty, and has
    // no way of knowing when it's detached. Eventually this should have the same behavior.
    if (path.length >= 2 && (path[0] == 'console' || path[0] == 'performance') && path[1] == 'memory')
        return;

    // Skip things that are assumed to be constants.
    if (path[path.length - 1].toUpperCase() == path[path.length - 1])
        return;

    // Various special cases for legacy reasons. Please do not add entries to this list.
    var propertyPath = path.join('.');
    switch (propertyPath) {
    case "location.href":
        expected = "'about:blank'";
        break;
    case "location.origin":
        expected = "'null'";
        break;
    case "location.pathname":
        expected = "'blank'";
        break;
    case "location.protocol":
        expected = "'about:'";
        break;
    case "navigator.appCodeName":
    case "navigator.appName":
    case "navigator.hardwareConcurrency":
    case "navigator.language":
    case "navigator.onLine":
    case "navigator.platform":
    case "navigator.product":
    case "navigator.productSub":
    case "navigator.vendor":
    case "navigator.connection.type":
        expected = "window." + propertyPath;
        break;
    case "screen.orientation":
        expected = "'portrait-primary'";
        break;
    }

    insertExpectedResult(path, expected);
}

function collectPropertiesHelper(object, path)
{
    if (path.length > 20)
        throw 'Error: probably looping';

    for (var property in object) {
        path.push(property);
        var type = typeof(object[property]);
        if (type == "object") {
            if (object[property] === null) {
                emitExpectedResult(path, "null");
            } else if (!object[property].Window
                && !(object[property] instanceof Node)
                && !(object[property] instanceof MimeTypeArray)
                && !(object[property] instanceof PluginArray)) {
                // Skip some traversing through types that will end up in cycles...
                collectPropertiesHelper(object[property], path);
            }
        } else if (type == "string") {
            emitExpectedResult(path, "''");
        } else if (type == "number") {
            emitExpectedResult(path, "0");
        } else if (type == "boolean") {
            emitExpectedResult(path, "false");
        }
        path.pop();
    }
}
