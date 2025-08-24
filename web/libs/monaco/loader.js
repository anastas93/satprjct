// Загрузчик Monaco Editor из локального каталога
export async function loadMonaco(sel){
  // подключаем AMD-загрузчик Monaco
  const s=document.createElement('script');
  s.src='./libs/monaco/min/vs/loader.js';
  document.head.appendChild(s);
  await new Promise(r=>s.onload=r);
  require.config({paths:{vs:'./libs/monaco/min/vs'}});
  return new Promise(res=>{
    require(['vs/editor/editor.main'],()=>{
      const el=document.querySelector(sel);
      // создаём редактор и сохраняем ссылку глобально
      window.monacoEditor=monaco.editor.create(el,{value:'',language:'javascript',automaticLayout:true});
      res(window.monacoEditor);
    });
  });
}
