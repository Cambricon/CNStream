function postData(post_url, data, sucessFunc, errorFunc, content_type=false) {
    $.ajax({
        type: "POST",
        url: post_url,
        data: data,
        contentType: content_type,
        processData: false,
        success: function (callback) {
            if (sucessFunc) {
                sucessFunc(callback);
            }
        },
        error: function(){
            if (errorFunc) {
                errorFunc();
            }
        }
    });
}

function getData(get_url, processCallback, intervalMethod) {
    $.get(get_url, function(callback){
        processCallback(callback, intervalMethod);
    });
}