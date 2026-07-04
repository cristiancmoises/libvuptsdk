use ml_kem::kem::{Decapsulate, Encapsulate};
use ml_kem::{Encoded, EncodedSizeUser, KemCore, MlKem768};
use std::{env, fs};
type Ek = <MlKem768 as KemCore>::EncapsulationKey;
type Dk = <MlKem768 as KemCore>::DecapsulationKey;
fn rh(p:&str)->Vec<u8>{hex::decode(fs::read_to_string(p).unwrap().trim()).unwrap()}
fn main(){
    let a:Vec<String>=env::args().collect();
    match a[1].as_str(){
        "keygen"=>{let mut r=rand::thread_rng();let(dk,ek)=MlKem768::generate(&mut r);
            fs::write("rk_ek.hex",hex::encode(ek.as_bytes())).unwrap();
            fs::write("rk_dk.hex",hex::encode(dk.as_bytes())).unwrap();
            println!("ek {}",ek.as_bytes().len());}
        "encap"=>{let ek=Ek::from_bytes(&Encoded::<Ek>::try_from(rh(&a[2]).as_slice()).unwrap());
            let mut r=rand::thread_rng();let(ct,ss)=ek.encapsulate(&mut r).unwrap();
            fs::write("rk_ct.hex",hex::encode(ct.as_slice())).unwrap();
            fs::write("rk_ss.hex",hex::encode(ss.as_slice())).unwrap();
            println!("ct {}",ct.as_slice().len());}
        "decap"=>{let dk=Dk::from_bytes(&Encoded::<Dk>::try_from(rh(&a[2]).as_slice()).unwrap());
            let ct=ml_kem::Ciphertext::<MlKem768>::try_from(rh(&a[3]).as_slice()).unwrap();
            let ss=dk.decapsulate(&ct).unwrap();println!("{}",hex::encode(ss.as_slice()));}
        _=>panic!("mode")
    }
}
